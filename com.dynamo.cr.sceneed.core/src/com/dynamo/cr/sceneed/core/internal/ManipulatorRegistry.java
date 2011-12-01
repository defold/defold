package com.dynamo.cr.sceneed.core.internal;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.runtime.IConfigurationElement;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.Plugin;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.sceneed.core.Activator;
import com.dynamo.cr.sceneed.core.IManipulator;
import com.dynamo.cr.sceneed.core.IManipulatorFactory;
import com.dynamo.cr.sceneed.core.IManipulatorInfo;
import com.dynamo.cr.sceneed.core.IManipulatorMode;
import com.dynamo.cr.sceneed.core.IManipulatorRegistry;

public class ManipulatorRegistry implements IManipulatorRegistry {

    Map<String, ManipulatorMode> modes = new HashMap<String, ManipulatorMode>();

    public void init(Plugin plugin) {
        IConfigurationElement[] config = Platform.getExtensionRegistry()
                .getConfigurationElementsFor("com.dynamo.cr.manipulators");
        try {
            for (IConfigurationElement e : config) {
                if (e.getName().equals("manipulator-mode")) {
                    String id = e.getAttribute("id");
                    String name = e.getAttribute("name");
                    ManipulatorMode mode = new ManipulatorMode(id, name);
                    modes.put(id, mode);
                } else if (e.getName().equals("manipulator")) {
                    String name = e.getAttribute("name");
                    String modeId = e.getAttribute("mode");
                    IManipulatorFactory factory = (IManipulatorFactory) e.createExecutableExtension("factory");

                    ManipulatorMode mode = modes.get(modeId);

                    ManipulatorInfo info = new ManipulatorInfo(name, mode, factory);
                    mode.addManipulatorInfo(info);
                }
            }
        } catch (Exception exception) {
            Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, exception.getMessage(), exception);
            plugin.getLog().log(status);
        }

    }

    @Override
    public IManipulatorMode getMode(String modeId) {
        return modes.get(modeId);
    }

    @Override
    public IManipulator getManipulatorForSelection(IManipulatorMode mode, Object[] selection) {
        List<IManipulatorInfo> list = mode.getManipulatorInfoList();
        for (IManipulatorInfo info : list) {
            IManipulatorFactory factory = info.getFactory();
            if (factory.match(selection)) {
                return factory.create();
            }
        }
        return null;
    }

}
