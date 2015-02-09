package com.sysdigcloud.sdjagent;

import sun.jvmstat.monitor.*;

import java.net.URISyntaxException;
import java.util.HashSet;
import java.util.List;
import java.util.ArrayList;
import java.util.Set;
import java.util.logging.Logger;

/**
 * Created by luca on 09/01/15.
 */
public class JvmstatVM {
    private final static Logger LOGGER = Logger.getLogger(JvmstatVM.class.getName());
    private final MonitoredVm vm;
    private static Set<Integer> cachedActiveVMs;
    private static final long activeVMRefreshInterval = 60 * 1000;
    private static long lastActiveVMRefresh;

    static {
        lastActiveVMRefresh = 0;
        cachedActiveVMs = new HashSet<Integer>();
    }

    public JvmstatVM(int pid) throws MonitorException {
        VmIdentifier vmId;
        try {
            vmId = new VmIdentifier(String.format("//%d", pid));
        } catch (URISyntaxException e) {
            // This exception should be very rare
            // rename it to MonitorException to avoid to deal with it
            // on throws clause
            throw new MonitorException(e);
        }
        MonitoredHost monitoredHost = MonitoredHost.getMonitoredHost(vmId);
        vm = monitoredHost.getMonitoredVm(vmId,-1);
    }

    public String findByName(String key)
    {
        String value = null;
        try
        {
            Monitor m = vm.findByName(key);
            if (m != null)
            {
                value = (String) m.getValue();
            }
        }
        catch ( MonitorException ex)
        {
            LOGGER.warning("MonitorException on JvmstatVM: " + ex.getMessage());
        }
        return value;
    }

    public List<String> findByPattern(String pattern)
    {
        try {
            List<Monitor> monitorList = vm.findByPattern(pattern);
            List<String> monitorStrList = new ArrayList<String>(monitorList.size());
            for (Monitor monitor : monitorList) {
                monitorStrList.add(monitor.getValue().toString());
            }
            return monitorStrList;
        } catch (MonitorException ex)
        {
            LOGGER.warning("MonitorException on JvmstatVM: " + ex.getMessage());
            return null;
        }
    }

    public String getJvmArgs() {
        try
        {
            return MonitoredVmUtil.jvmArgs(vm);
        } catch (MonitorException ex)
        {
            LOGGER.warning("MonitorException on JvmstatVM: " + ex.getMessage());
            return null;
        }
    }

    public String getMainClass()
    {
        try
        {
            return MonitoredVmUtil.mainClass(vm, true);
        } catch (MonitorException ex)
        {
            LOGGER.warning("MonitorException on JvmstatVM: " + ex.getMessage());
            return null;
        }
    }

    public static Set<Integer> getActiveVMs() {
        if(System.currentTimeMillis() - lastActiveVMRefresh > activeVMRefreshInterval) {
            cachedActiveVMs.clear();
            try {
                String hostname = null;
                MonitoredHost localHost = MonitoredHost.getMonitoredHost(new HostIdentifier(hostname));
                cachedActiveVMs = localHost.activeVms();
            } catch (URISyntaxException ex) {
                LOGGER.warning("URISyntaxException on JvmstatVM: " + ex.getMessage());
            } catch (MonitorException ex) {
                LOGGER.warning("MonitorException on JvmstatVM: " + ex.getMessage());
            }
            lastActiveVMRefresh = System.currentTimeMillis();
        }
        return cachedActiveVMs;
    }
}
