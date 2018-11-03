--[[
Copyright (C) 2013-2018 Draios Inc dba Sysdig.

This file is part of sysdig.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

--]]

view_info = 
{
	id = "kubernetes_controllers",
	name = "K8s Controllers",
	description = "List all Kubernetes controllers running on this machine, and the resources that each of them uses.",
	tips = {"Select a controller and click enter to drill down into it. At that point, you will be able to access several views that will show you the details of the selected controller."},
	view_type = "table",
	applies_to = {"", "evt.res", "k8s.pod.id", "k8s.svc.id", "k8s.ns.id"},
	filter = "k8s.rc.id != ''",
	use_defaults = true,
	drilldown_target = "kubernetes_pods",
	columns = 
	{
		{
			name = "NA",
			field = "thread.tid",
			is_key = true
		},
		{
			name = "CPU",
			field = "thread.cpu",
			description = "Amount of CPU used by the controller.",
			colsize = 8,
			aggregation = "AVG",
			groupby_aggregation = "SUM",
			is_sorting = true
		},
		{
			name = "VIRT",
			field = "thread.vmsize.b",
			description = "Total virtual memory for the controller.",
			aggregation = "MAX",
			groupby_aggregation = "SUM",
			colsize = 9
		},
		{
			name = "RES",
			field = "thread.vmrss.b",
			description = "Resident non-swapped memory for the controller.",
			aggregation = "MAX",
			groupby_aggregation = "SUM",
			colsize = 9
		},
		{
			name = "FILE",
			field = "evt.buflen.file",
			description = "Total (input+output) file I/O bandwidth generated by the controller, in bytes per second.",
			colsize = 8,
			aggregation = "TIME_AVG",
			groupby_aggregation = "SUM"
		},
		{
			name = "NET",
			field = "evt.buflen.net",
			description = "Total (input+output) network bandwidth generated by the controller, in bytes per second.",
			colsize = 8,
			aggregation = "TIME_AVG",
			groupby_aggregation = "SUM"
		},
		{
			name = "NA",
			field = "k8s.rc.id",
			is_groupby_key = true
		},
		{
			name = "ID",
			field = "k8s.rc.id",
			description = "Controller id.",
			colsize = 38
		},
		{
			name = "NAME",
			field = "k8s.rc.name",
			description = "Controller name.",
			colsize = 25
		},
		{
			name = "NAMESPACE",
			field = "k8s.ns.name",
			description = "Controller namespace.",
			colsize = 20
		},
		{
			name = "LABELS",
			field = "k8s.rc.labels",
			description = "Controller labels.",
			colsize = 0
		},
	},
	actions = 
	{
		{
			hotkey = "d",
			command = "kubectl --namespace=%k8s.ns.name describe rc %k8s.rc.name",
			description = "describe"
		},
		{
			hotkey = "x",
			command = "kubectl --namespace=%k8s.ns.name delete rc %k8s.rc.name",
			description = "delete"
		}
	}
}