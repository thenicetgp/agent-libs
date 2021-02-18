package kubecollect_common

import (
	"golang.org/x/net/context"
	"io/ioutil"
	tw "k8s.io/client-go/tools/watch"
	"math/rand"
	"reflect"
	"regexp"
	"sort"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/apimachinery/pkg/util/runtime"
	"k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/cache"
	"k8s.io/client-go/tools/clientcmd"

	apierrs "k8s.io/apimachinery/pkg/api/errors"
	v1meta "k8s.io/apimachinery/pkg/apis/meta/v1"
	discovery "k8s.io/client-go/discovery"
	kubeclient "k8s.io/client-go/kubernetes"
	clientcmdapi "k8s.io/client-go/tools/clientcmd/api"

	"cointerface/profile"
	"github.com/draios/protorepo/sdc_internal"
	draiosproto "protorepo/agent-be/proto"

	log "github.com/cihub/seelog"
	"github.com/gogo/protobuf/proto"
)

const (
	EVENT_ADD = iota
	EVENT_UPDATE = iota
	EVENT_UPDATE_AND_SEND = iota
	EVENT_DELETE = iota
)

// stores which informers have been allocated, so at least we don't segfault
// does not imply we've successfully registered with the API server
var StartedMap map[string]bool
var StartedMutex sync.RWMutex

var receiveMap map[string]bool
var receiveMutex sync.RWMutex
var annotFilter map[string]bool

var eventCountsLogTime uint32
var eventMapAdd map[string]int
var emAddMutex sync.RWMutex
var eventMapUpd map[string]int
var emUpdMutex sync.RWMutex
var eventMapUpds map[string]int
var emUpdsMutex sync.RWMutex
var eventMapDel map[string]int
var emDelMutex sync.RWMutex

const (
	ChannelTypeInformer = iota
	ChannelTypeUserEvent = iota
)

// Given that we depend on global contexts, we need a hard guarantee that we won't have
// two users trying to attach to channels at the same time.
var ChannelMutex sync.Mutex

var InformerChannelInUse = false
var InformerChannel chan draiosproto.CongroupUpdateEvent

func AddEvent(restype string, evtype int) {
	profile.NewEvent()
	if (eventCountsLogTime < 1) {
		return
	}
	if (evtype == EVENT_ADD) {
		emAddMutex.Lock()
		eventMapAdd[restype] = eventMapAdd[restype] + 1
		emAddMutex.Unlock()
	} else if (evtype == EVENT_UPDATE) {
		emUpdMutex.Lock()
		eventMapUpd[restype] = eventMapUpd[restype] + 1
		emUpdMutex.Unlock()
	} else if (evtype == EVENT_UPDATE_AND_SEND) {
		emUpdsMutex.Lock()
		eventMapUpds[restype] = eventMapUpds[restype] + 1
		emUpdsMutex.Unlock()
	} else if (evtype == EVENT_DELETE) {
		emDelMutex.Lock()
		eventMapDel[restype] = eventMapDel[restype] + 1
		emDelMutex.Unlock()
	} else {
		log.Warnf("addEvent, unknown event type %d", evtype)
	}
}

func logEvents() {
	if (eventCountsLogTime < 1) {
		return
	}

	emAddMutex.RLock()
	emUpdMutex.RLock()
	emUpdsMutex.RLock()
	emDelMutex.RLock()

	for k := range eventMapAdd {
		log.Infof("%s Events: %d adds, %d updates, %d updates sent, %d deletes",
			  k,
			  eventMapAdd[k],
			  eventMapUpd[k],
			  eventMapUpds[k],
			  eventMapDel[k])
	}

	emDelMutex.RUnlock()
	emUpdsMutex.RUnlock()
	emUpdMutex.RUnlock()
	emAddMutex.RUnlock()
}

const RsyncInterval = 10 * time.Minute

func InSortedArray(s string, arr []string) bool {
	index := sort.SearchStrings(arr, s)

	if index < len(arr) && arr[index] == s {
		return true
	}
	return false
}

func addCustomResources(resources []*v1meta.APIResourceList, customResources []string) ([]*v1meta.APIResourceList) {
	if len(customResources) > 0 {

		// Create a sysdig/v1alpha1 group version
		sysdigResources := v1meta.APIResourceList{
			TypeMeta: v1meta.TypeMeta{
				Kind:       "",
				APIVersion: "",
			},
			GroupVersion: "sysdig/v1alpha1",
			APIResources: nil,
		}

		// Add custom resources to sysdig new group version
		for _,customResource := range customResources {
			sysdigResources.APIResources = append(sysdigResources.APIResources, v1meta.APIResource{Name: customResource})
		}

		// Add the new group version to the list of resources
		resources = append(resources, &sysdigResources)
	}
	return resources
}

func GetResourceTypes(resources []*v1meta.APIResourceList, includeTypes []string, checkIncludeOptional ...bool) ([]string) {

	checkInclude := true // True by default
	if len(checkIncludeOptional) > 0 {
		checkInclude = checkIncludeOptional[0]
	}
	// Return a vector of all resourceType names
	var resourceTypes []string
	resourceMap := make(map[string]bool)

	havePods := false

	customResources := []string {"podstatuscounter"}

	resources = addCustomResources(resources, customResources)

	defaultDisabledResources := []string {
		"services",
		"resourcequotas",
		"horizontalpodautoscalers",
		"persistentvolumes",
		"persistentvolumeclaims",
	}
	for _, nsr := range customResources {
		defaultDisabledResources = append(defaultDisabledResources, nsr)
	}

	sort.Strings(defaultDisabledResources)
	sort.Strings(includeTypes)

	for _, resourceList := range resources {
		for _, resource := range resourceList.APIResources {
			verbStr := ""
			for _, verb := range resource.Verbs {
				verbStr += verb
				verbStr += ","
			}
			verbStr = strings.Trim(verbStr, ",")
			log.Debugf("K8s API server supports %s/%s: %s",
				resourceList.GroupVersion, resource.Name, verbStr)

			if resource.Name == "cronjobs" &&
				resourceList.GroupVersion != "batch/v1beta1" {
				continue
			}
			// Exclude services, rqs, hpas, pvs and pvcs unless explicitly requested
			// We'll probably want to change this
			// Note that PVCs may depend on PVs
			if checkInclude && InSortedArray(resource.Name, defaultDisabledResources) && !InSortedArray(resource.Name, includeTypes) {
				log.Debugf("K8s: Exclude resourcetype %s", resource.Name)
				continue
			}

			if(!resourceMap[resource.Name]) {
				// This resource hasn't been added. Added it now
				resourceMap[resource.Name] = true

				// If the resource type is "nodes" or "namespaces" we
				// PREPEND them. (we want to process those first). Else
				// append the other resource types.
				if(resource.Name == "nodes" || resource.Name == "namespaces") {
					resourceTypes = append([]string{resource.Name}, resourceTypes...)
				} else if(resource.Name == "pods") {
					havePods = true
				} else {
					resourceTypes = append(resourceTypes, resource.Name)
				}
			}
		}
	}

	if havePods {
		resourceTypes = append(resourceTypes, "pods")
	}

	return resourceTypes
}

func getServerDiscoveryResources(dI discovery.DiscoveryInterface) ([]*v1meta.APIResourceList , error) {
	resources, err := dI.ServerResources()

	if err != nil {
		// First print the error that occured during resources discovery
		log.Infof("K8s server returned [ %s ] during resources discovery", err)

		// Next, we decide to continue or not, based on 2 things:
		// 1.) Did we find some resources in our discovery?
		// 2.) Was this error of kind "ErrGroupDiscoveryFailed"
		if len(resources) == 0 || !(discovery.IsGroupDiscoveryFailedError(err)) {
			// No resources were discovered.
			// OR -- We did find resources , but the error is not of type "ErrGroupDiscoveryFailed"
			// We should exit. Ideally this should never be hit (According to current impl of ServerResources). See:
			// https://github.com/kubernetes/client-go/blob/67a413f31aeaaff8cd9352ad060c1e57232157c8/discovery/discovery_client.go#L281
			return nil, err
		}
		// In other cases, continue
		// Display a list of resources discovered. Use the getResourceTypes method for this
		// For the purpose of logging include all discovered types
		log.Infof("Continuing k8s set up with the resources that were discovered: %v", GetResourceTypes(resources, nil, false))
	}
	return resources, nil
}

// Generic function used to drain any receive chan( <-chan)
// This method ensures that by fully draining the chan,
// we help unblock any other routines/methods that are blocked
// on sending on these chans. This is called during cleanup
func DrainChan(in interface{}) {
	log.Debugf("[DrainChan]: Entering drain chan loop")

	cin := reflect.ValueOf(in)
	if cin.Kind() != reflect.Chan {
		log.Warnf("[DrainChan]: can't drain a : %v", cin.Kind())
		return
	}
	if (cin.Type()).ChanDir() != reflect.RecvDir {
		log.Warnf("[DrainChan]: can't drain a chan other than RecvDir Chan: %v",(cin.Type()).ChanDir().String())
		return
	}

	for {
		x, ok := cin.Recv()
		if !ok {
			log.Debugf("[DrainChan]: end of draining chan")
			return
		}
		log.Debugf("[DrainChan]: draining : %v", x)
	}
}

type gKubeClientStruct struct {
	client kubeclient.Interface
	mutex sync.Mutex
	clientChan chan struct {}
}

var gKubeClient gKubeClientStruct

func GetKubeClient() (kubeclient.Interface, chan struct{}) {
	gKubeClient.mutex.Lock()
	kc := gKubeClient.client
	kcc := gKubeClient.clientChan
	gKubeClient.mutex.Unlock()
	return kc, kcc
}

func setKubeClient(kc kubeclient.Interface, kcc chan struct{}) {
	gKubeClient.mutex.Lock()
	if (gKubeClient.clientChan != nil) {
		log.Info("Closing k8s client channel")
		// Close the existing client channel to notify readers
		// Presuming GC will clean up the actual client when all refs are gone
		close(gKubeClient.clientChan)
	}
	gKubeClient.client = kc
	gKubeClient.clientChan = kcc
	gKubeClient.mutex.Unlock()
}

func CloseKubeClient() {
	setKubeClient(nil, nil)
}

// The input context is passed to all goroutines created by this function.
// The caller is responsible for draining messages from the returned channel
// until the channel is closed, otherwise the component goroutines may block.
// The empty struct chan notifies the caller that the initial event fetch
// is complete by closing the chan.
func WatchCluster(parentCtx context.Context, opts *sdc_internal.OrchestratorEventsStreamCommand, kubecollectInterface KubecollectInterface) (<-chan sdc_internal.ArrayCongroupUpdateEvent, <-chan struct{}, error) {
	setErrorLogHandler()

	// TODO: refactor error messages
	var kubeClient kubeclient.Interface

	if opts.GetMaxRndConnDelay() != 0 {
		rand.Seed(time.Now().UnixNano())
		delay := rand.Uint32() % opts.GetMaxRndConnDelay()
		log.Infof("Waiting to connect to k8s server for %d seconds", delay)
		time.Sleep(time.Duration(delay) * time.Second)
	}

	if opts.GetUrl() != "" {
		log.Infof("Connecting to k8s server at %s", opts.GetUrl())
		var err error
		kubeClient, err = createKubeClient(opts.GetUrl(),
						   opts.GetCaCert(),
						   opts.GetClientCert(),
						   opts.GetClientKey(),
						   opts.GetSslVerifyCertificate(),
						   opts.GetAuthToken())
		if err != nil {
			InformerChannelInUse = false
			log.Errorf("Cannot create k8s client: %s", err)
			return nil, nil, err
		}
	} else {
		log.Infof("Connecting to k8s server using inCluster config")
		var err error
		kubeClient, err = createInClusterKubeClient()
		if err != nil {
			InformerChannelInUse = false
			log.Errorf("Cannot create k8s client: %s", err)
			return nil, nil, err
		}
	}
	log.Infof("Testing communication with server")
	srvVersion, err := kubeClient.Discovery().ServerVersion()
	if err != nil {
		InformerChannelInUse = false
		log.Errorf("K8s server not responding: %s", err)
		return nil, nil, err
	}
	log.Infof("Communication with server successful: %v", srvVersion)

	// Get the resources discovered thru the discovery service.
	resources, err := getServerDiscoveryResources(kubeClient.Discovery())
	if err != nil {
		log.Errorf("K8s resource discovery returned an error: %s", err)
		InformerChannelInUse = false
		return nil, nil, err
	}

	// Set global kubeClient for use by events stream
	setKubeClient(kubeClient, make(chan struct {}))

	// These get reset when either events or listeners channel is reset
	StartedMap = make(map[string]bool)
	receiveMap = make(map[string]bool)
	SetAnnotFilt( opts.AnnotationFilter)

	eventMapAdd = make(map[string]int)
	eventMapUpd = make(map[string]int)
	eventMapUpds = make(map[string]int)
	eventMapDel = make(map[string]int)
	eventCountsLogTime = opts.GetEventCountsLogTime()
	log.Infof("Event Counts log time: %d s", eventCountsLogTime)


	// Get a vector of all resource types
	// from the resourceList in resources.
	resourceTypes := GetResourceTypes(resources, opts.IncludeTypes)

	// Make a channel that takes in an Array of CongroupUpdateEvents
	// Make it of capacity 1. At any given instance we should send
	// only 1 event on it. This happens in batchEvents
	// What this ensures is that the calculation for length of how
	// many events in the queue becomes trivial in startInformers
	// Else we will be forced to perform a deep len calculation. 
	evtArrayChan := make(chan sdc_internal.ArrayCongroupUpdateEvent, 1)

	// Batch cointerface messages options
	// Get the config values for batching cointerface msgs and sanity check the values.
	batchMsgsQueueLen := opts.GetBatchMsgsQueueLen()
	if(batchMsgsQueueLen <= 0) {
		log.Warnf("A value less than 1 entered for the orch_batch_msgs_queue_len configuration property. Setting the value to 1.")
		batchMsgsQueueLen = 1
	}
	batchMsgsTickMs := opts.GetBatchMsgsTickIntervalMs()
	if(batchMsgsTickMs <= 0) {
		log.Warnf("A value less than 1 entered for the orch_batch_msgs_tick_interval configuration property. Setting the value to 1.")
		batchMsgsTickMs = 1
	}

	ctx, cancel := context.WithCancel(parentCtx)
	// Start a routine to do a watch on namespaces
	// to detect api server connection errors because
	// SharedInformers don't surface errors
	//
	// Returns synchronously with err set if the initial watch fails
	// Else, return nil and spawn a goroutine to monitor the watch
	err = startWatchdog(parentCtx, cancel, kubeClient)
	if err != nil {
		InformerChannelInUse = false
		// startWatchdog() may later hit an async error,
		// so it's responsible for all error logging
		return nil, nil, err
	}

	InformerChannel = make(chan draiosproto.CongroupUpdateEvent,
			       opts.GetQueueLen())

	fetchDone := make(chan struct{})
	var wg sync.WaitGroup

	// A var that will be accessed atomically in both
	// batchEvents and startInformers. This var will hold
	// the length of the sdcEvtArray at any given time.
	queueLength := uint32(0)

	// Start informers in a separate routine so we can return the
	// evt chan and let the below goroutine start reading/draining events
	go kubecollectInterface.StartInformers(ctx, kubeClient, &wg, fetchDone, opts, resourceTypes, &queueLength)

	// as soon as we start the go routine to start informers;
	// we need to kick off the routine to start reading events
	// from Informerchannel and then batching them into an array and sending
	// that array on the evtArrayChan
	go BatchEvents(ctx, evtArrayChan, batchMsgsQueueLen, batchMsgsTickMs, &queueLength)

	if (eventCountsLogTime > 0) {
		go func() {
			for {
				time.Sleep(time.Duration(eventCountsLogTime) * time.Second)
				logEvents()
			}
		}()
	}

	return evtArrayChan, fetchDone, nil
}

func startWatchdog(parentCtx context.Context, cancel context.CancelFunc, kubeClient kubeclient.Interface) error {
	log.Debug("Starting K8s watchdog")

	client := kubeClient.CoreV1().RESTClient()
	// We don't care about what we watch, so limit to a single namespace
	fSelector, _ := fields.ParseSelector("metadata.name=default")
	lw := cache.NewListWatchFromClient(client, "namespaces", v1meta.NamespaceAll, fSelector)
	watcher, err := lw.Watch(v1meta.ListOptions{})
	if err != nil {
		fullErrString := err.Error()
		// Watch errors report "unknown" in some cases where the list
		// error is more descriptive, so check if there's a list error
		// XXX this may be fixed by upgrading client-go
		if strings.HasPrefix(fullErrString, "unknown") {
			_, listErr := lw.List(v1meta.ListOptions{})
			if listErr != nil {
				fullErrString += ", additional details: " + listErr.Error()
			}
		}

		log.Errorf("K8s watchdog, error creating api server watchdog: %v", fullErrString)
		cancel()
		return err
	}
	// defer watcher.Stop() from the new goroutine

	go func() {
		log.Debug("Creating K8s watchdog thread")

		// cancel() unless we timeout and successfully create a new watchdog
		doCancel := true
		defer func() {
			log.Debug("K8s watchdog thread exiting")
			watcher.Stop()
			if doCancel {
				cancel()
			}
		}()

		for {
			select {
			case event, ok := <-watcher.ResultChan():
				if !ok {
					// The API server can timeout the watch during normal,
					// operation, so launch a new watchdog connection
					log.Debug("K8s watchdog received a watch timeout, restarting")
					err := startWatchdog(parentCtx, cancel, kubeClient)
					if err == nil {
						doCancel = false
					}
					return
				}
				if event.Type == watch.Error {
					log.Errorf("K8s watchdog received watch error: %v",
						apierrs.FromObject(event.Object))
					return
				}
			case <-parentCtx.Done():
				log.Info("K8s watchdog, parent context cancelled")
				return
			}
		}
	}()

	return nil
}

// Go routine that is responsible for reading events off the Informer channel
// and batching them into an array of CongroupUpdateEvents. Then send this
// array on the evtArrayChan.
// Perform this periodically (based on a timer) or if the array is full.
func BatchEvents(
	ctx context.Context,
	evtArrayChan chan<- sdc_internal.ArrayCongroupUpdateEvent,
	batchMsgsQueueLen uint32,
	batchMsgsTickMs uint32,
	queueLength *uint32) {

	// Periodic timer to drain the events
	msTickTimer := time.NewTicker(time.Duration(batchMsgsTickMs) * time.Millisecond)
	defer msTickTimer.Stop()

	// before reading defer a function to completely
	// drain Informer channel to unblock informers
	// Also close the evtArrayChan
	defer func() {
		// Drain the Informer channel fully
		// Cast it to a receiver-only chan before sending to DrainChan
		DrainChan((<-chan draiosproto.CongroupUpdateEvent)(InformerChannel))

		// Close the evtArrayChan
		close(evtArrayChan)
	}()

	sdcEvtArray := sdc_internal.ArrayCongroupUpdateEvent{
		Events: make([]*draiosproto.CongroupUpdateEvent, 0, batchMsgsQueueLen)}

	for {
		timerTick := false
		select {
		case evt, ok := <-InformerChannel:
			if !ok {
				log.Debugf("[PerformOrchestratorEventsStream] event stream is closed")
				return
			}

			// append incoming evt to the sdcEvtArray's Event field
			sdcEvtArray.Events = append(sdcEvtArray.Events, &evt)
		case <-msTickTimer.C:
			// We drain the event queue every batchMsgsTickMs milliseconds
			// This prevents msgs that are old to wait around
			// forever if we don't fill the event queue
			// Here we just set a boolean flag to let the flush code (at the
			// end of the select know that we are flushing due to timer)
			// 
			timerTick = true
		case <-ctx.Done():
			log.Debugf("[PerformOrchestratorEventsStream] context cancelled")
			return
		}

		// We need to flush in 1 of 2 cases:
		// 1.) Either we reached full capacity of the queue
		// 2.) Or timer tick went off 
		if((len(sdcEvtArray.Events) >= int(batchMsgsQueueLen)) ||
			(timerTick && (len(sdcEvtArray.Events) > 0))) {
			evtArrayChan <- sdcEvtArray
			// Now reset this sdcEvtArray before reuse
			sdcEvtArray = sdc_internal.ArrayCongroupUpdateEvent{
				Events: make([]*draiosproto.CongroupUpdateEvent, 0, batchMsgsQueueLen)}

			// Reset timerTick to false always
			timerTick = false
		}
		// Write out the length before leaving this current select cycle
		atomic.StoreUint32(queueLength, uint32(len(sdcEvtArray.Events)))
	}
}

func EventReceived(resource string) {
	receiveMutex.Lock()
	receiveMap[resource] = true
	receiveMutex.Unlock()
}

func ReceivedEvent(resource string) bool {
	receiveMutex.RLock()
	ret := receiveMap[resource]
	receiveMutex.RUnlock()
	return ret
}

func ResourceReady(resource string) bool {
	StartedMutex.RLock()
	ret := StartedMap[resource]
	StartedMutex.RUnlock()
	return ret
}

func createKubeClient(apiServer string, caCert string, clientCert string, clientKey string, sslVerify bool, authToken string) (kubeClient kubeclient.Interface, err error) {
	skipVerify := !sslVerify
	if skipVerify {
		caCert = ""
	}
	tokenStr := ""
	if authToken != "" {
		tokenBytes, err := ioutil.ReadFile(authToken)
		if err != nil {
			log.Warnf("Unable to read bearer token from %v", authToken)
		} else {
			tokenStr = string(tokenBytes[:])
			// Trailing newlines cause the api server to reject the token
			tokenStr = strings.TrimRight(tokenStr, "\n")
			if tokenStr == "" {
				log.Warn("No token found in bearer token file")
			}
		}
	}

	baseConfig := clientcmdapi.NewConfig()
	configOverrides := &clientcmd.ConfigOverrides{
		ClusterInfo: clientcmdapi.Cluster{
			Server: apiServer,
			InsecureSkipTLSVerify: skipVerify,
			CertificateAuthority: caCert,
		},
		AuthInfo: clientcmdapi.AuthInfo{
			ClientCertificate: clientCert,
			ClientKey: clientKey,
			Token: tokenStr,
		},
	}
	kubeConfig := clientcmd.NewDefaultClientConfig(*baseConfig, configOverrides)
	config, err := kubeConfig.ClientConfig()
	if err != nil {
		log.Errorf("kubecollect can't create config")
		return nil, err
	}

	kubeClient, err = kubeclient.NewForConfig(config)
	if err != nil {
		log.Errorf("kubecollect NewForConfig fails")
		return nil, err
	}

	return kubeClient, nil
}

func createInClusterKubeClient() (kubeClient kubeclient.Interface, err error) {
	config, err := rest.InClusterConfig()
	if err != nil {
		log.Errorf("Cannot create InCluster config: %s", err)
		return nil, err
	}
	log.Debugf("InCluster k8s server: %s", config.Host);
	// creates the clientset
	kubeClient, err = kubeclient.NewForConfig(config)
	if err != nil {
		log.Errorf("Cannot create client using cluster config, server %s: %s",
				   config.Host, err)
		return nil, err
	}
	return
}

// The default logger for client-go errors logs them to stderr in a format
// the C++ side can't parse, so intercept them and re-log them correctly
func setErrorLogHandler() {
	// Remove the leading filename + line number
	// Example: "cointerface/kubecollect/pods.go:123: "
	errRegex, err := regexp.Compile(`^cointerface/\S+\.go:\d+: `)
	if err != nil {
		log.Errorf("Unable to create error log regex: %v", err)
		return
	}

	// We intentionally reassign ErrorHandlers so it both
	// adds our handler and removes the existing handlers
	runtime.ErrorHandlers = []func(error) {
		func(err error) {
			startIdx := 0
			loc := errRegex.FindStringIndex(err.Error())
			if loc != nil {
				startIdx = loc[1]
			}
			log.Error(err.Error()[startIdx:])
		},
	}
}

func GetTags(obj v1meta.ObjectMeta, prefix string) map[string]string {
	tags := make(map[string]string)
	for k, v := range obj.GetLabels() {
		tags[prefix+"label." + k] = v
	}
	tags[prefix+"name"] = obj.GetName()
	return tags
}

func GetLabelSelector(labelSelector v1meta.LabelSelector) *draiosproto.K8SLabelSelector {
	matchExpressions := make([]*draiosproto.K8SLabelSelectorRequirement, 0, len(labelSelector.MatchExpressions))

	for _, e := range labelSelector.MatchExpressions {
		matchExpressions = append(matchExpressions, &draiosproto.K8SLabelSelectorRequirement{
			Key: proto.String(e.Key),
			MatchOperator: proto.String(string(e.Operator)),
			Values: e.Values,
		})
	}
	return &draiosproto.K8SLabelSelector{
		MatchLabels: labelSelector.MatchLabels,
		MatchExpressions: matchExpressions,
	}
}

// This needs to be called before any informers are started as the map is
// not thread-safe for mixing reads & writes.
func SetAnnotFilt(annots []string) {
	if len(StartedMap) != 0 {
		log.Error("Writing to annotation filter map after multi-threading start")
	}
	annotFilter = make(map[string]bool)
	for _, v := range annots {
		annotFilter[v] = true
	}
}

func GetProbes(pod *v1.Pod) map[string]string {
	tags := make(map[string]string)

	for _, container := range pod.Spec.Containers {
		containerid := ""
		if pod.Status.ContainerStatuses == nil {
			break
		}
		for _, containerStatus := range pod.Status.ContainerStatuses {
			if containerStatus.Name == container.Name {
				containerid, _ = ParseContainerID(containerStatus.ContainerID)
				break
			}
		}

		if container.LivenessProbe != nil {
			k := "kubernetes.pod.probe.liveness." + containerid
			v := ""
			if container.LivenessProbe.Handler.Exec != nil {
				for c, cmd := range container.LivenessProbe.Handler.Exec.Command {
					if c != 0 {
						v = v + " "
					}
					v = v + cmd
				}
				tags[k] = v
			}
		}

		if container.ReadinessProbe != nil {
			k := "kubernetes.pod.probe.readiness." + containerid
			v := ""
			if container.ReadinessProbe.Handler.Exec != nil {
				for c, cmd := range container.ReadinessProbe.Handler.Exec.Command {
					if c != 0 {
						v = v + " "
					}
					v = v + cmd
				}
				tags[k] = v
			}
		}
	}
	if len(tags) == 0 {
		return nil
	}
	return tags
}

func GetAnnotations(obj v1meta.ObjectMeta, prefix string) map[string]string {
	if len(annotFilter) == 0 {
		return nil
	}
	tags := make(map[string]string)
	for k, v := range obj.GetAnnotations() {
		// Only get selected annotations
		annot := prefix + "annotation." + k
		if annotFilter[annot] {
			tags[annot] = v
		}
	}
	if len(tags) == 0 {
		return nil
	}
	return tags
}

func MergeInternalTags(m1 map[string]string, m2 map[string]string) map[string]string {
	if m1 == nil && m2 == nil {
		return nil
	}
	if m1 == nil {
		return m2
	}
	if m2 == nil {
		return m1
	}
	for k, v := range m2 {
		m1[k] = v
	}
	return m1
}

func EqualLabels(lhs v1meta.ObjectMeta, rhs v1meta.ObjectMeta) bool {
	left := lhs.GetLabels()
	right := rhs.GetLabels()
	if (len(left) != len(right)) {
		return false
	}
	for k,v := range left {
		if right[k] != v {
			return false
		}
	}
	return true
}

func EqualAnnotations(lhs v1meta.ObjectMeta, rhs v1meta.ObjectMeta) bool {
	if len(annotFilter) == 0 {
		return true
	}
	left := lhs.GetAnnotations()
	right := rhs.GetAnnotations()
	if (len(left) != len(right)) {
		return false
	}
	for k,v := range left {
		if right[k] != v {
			return false
		}
	}
	return true
}

func EqualProbes(lhs *v1.Pod, rhs *v1.Pod) bool {
	left := GetProbes(lhs)
	right := GetProbes(rhs)

	if len(left) != len(right) {
		return false
	}

	for k, v := range left {
		if right[k] != v {
			return false
		}
	}

	return true
}

func EqualResourceList(lhs v1.ResourceList, rhs v1.ResourceList) bool {
	if len(lhs) != len(rhs) {
		return false
	}

	for k, lhsVal := range lhs {
		rhsVal, ok := rhs[k]
		if !ok || rhsVal.Cmp(lhsVal) != 0 {
			return false
		}
	}

	return true
}

func AppendMetric(metrics *[]*draiosproto.AppMetric, name string, val float64) {
	*metrics = append(*metrics, &draiosproto.AppMetric{
		Name:proto.String(name),
		Type:draiosproto.AppMetricType_APP_METRIC_TYPE_GAUGE.Enum(),
		Value:proto.Float64(val),
	})
}

func AppendRateMetric(metrics *[]*draiosproto.AppMetric, name string, val float64) {
	*metrics = append(*metrics, &draiosproto.AppMetric{
		Name:proto.String(name),
		Type:draiosproto.AppMetricType_APP_METRIC_TYPE_RATE.Enum(),
		Value:proto.Float64(val),
	})
}

func AppendMetricInt64(metrics *[]*draiosproto.AppMetric, name string, val int64) {
	AppendMetric(metrics, name, float64(val))
}

func AppendMetricInt32(metrics *[]*draiosproto.AppMetric, name string, val int32) {
	AppendMetric(metrics, name, float64(val))
}

func AppendMetricPtrInt32(metrics *[]*draiosproto.AppMetric, name string, val *int32) {
	v := int32(0)
	if val != nil {
		v = *val
	}
	AppendMetricInt32(metrics, name, v)
}

func AppendMetricBool(metrics *[]*draiosproto.AppMetric, name string, val bool) {
	v := float64(0)
	if val == true {
		v = 1
	}
	AppendMetric(metrics, name, v)
}

func AppendMetricResource(metrics *[]*draiosproto.AppMetric, name string, rList v1.ResourceList, rName v1.ResourceName) {
	v := float64(0)
	qty, ok := rList[rName]
	if ok {
		// Take MilliValue() and divide because
		// we could lose precision with Value()
		v = float64(qty.MilliValue())/1000
	}
	AppendMetric(metrics, name, v)
}

var OwnerRefKindToCongroupKind = map[string]string{
	"ReplicaSet":            "k8s_replicaset",
	"ReplicationController": "k8s_replicationcontroller",
	"StatefulSet":           "k8s_statefulset",
	"DaemonSet":             "k8s_daemonset",
	"Deployment":            "k8s_deployment",
	"Job":                   "k8s_job",
	"Node":                  "k8s_node",
}

func OwnerReferencesToParents(owners []v1meta.OwnerReference,
	parents *[]*draiosproto.CongroupUid,
	skip *map[string]bool) {

	var kind string
	for _, owner := range owners {
		if skip != nil {
			if _, ok := (*skip)[owner.Kind]; ok {
				continue
			}
		}

		if congroupKind, ok := OwnerRefKindToCongroupKind[owner.Kind]; ok {
			kind = congroupKind
		} else {
			kind = owner.Kind
		}

		*parents = append(*parents, &draiosproto.CongroupUid{
			Kind: proto.String(kind),
			Id:   proto.String(string(owner.UID)),
		})
	}
}


func StartWatcher(ctx context.Context,
	restClient rest.Interface,
	resource string,
	wg *sync.WaitGroup,
	evtc chan<- draiosproto.CongroupUpdateEvent,
	fieldSelector fields.Selector,
	handler func(event watch.Event, evtc chan<- draiosproto.CongroupUpdateEvent) ()) {

	lw := cache.NewListWatchFromClient(restClient, resource, v1meta.NamespaceAll, fieldSelector)

	wg.Add(1)

	go func() {
		defer func() {
			wg.Done()
		}()
		for {
			terminated := false
			select {
			case <- ctx.Done():
				terminated = true
			default:
			}

			log.Debugf("Starting retryWatcher %s", resource)
			_, err := tw.ListWatchUntil(ctx, lw,
				func(event watch.Event) (bool, error) {
					if event.Type == watch.Error {
						log.Warnf("startWatcher[%s] got event type Error", resource)
					} else {
						handler(event, evtc)
					}

					// Condition false means continue
					return false, nil
				})

			if err != nil {
				log.Warnf("startWatcher[%s] ListWatchUntil exits: %s", resource, err.Error())
			}

			time.Sleep(5 * time.Second)
			if terminated == true {
				break
			}
		}
	}()
}

func MapInsert(m *map[string]string, key string, value string) {
	if *m == nil {
		*m = make(map[string]string)
	}
	(*m)[key] = value
}

func GetPkg(kubecollectInterface KubecollectInterface) string {
	return reflect.TypeOf(kubecollectInterface).Name()
}
