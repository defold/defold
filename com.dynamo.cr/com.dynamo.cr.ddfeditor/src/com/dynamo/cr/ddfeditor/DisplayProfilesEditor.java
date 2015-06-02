package com.dynamo.cr.ddfeditor;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.ui.statushandlers.StatusManager;
import com.dynamo.render.proto.Render.DisplayProfile;
import com.dynamo.render.proto.Render.DisplayProfiles;

import java.util.HashSet;
import java.util.Set;

public class DisplayProfilesEditor extends DdfEditor {

    public DisplayProfilesEditor() {
        super("display_profiles");
    }

    private boolean validateProfiles() {
        DisplayProfiles profiles = (DisplayProfiles) getMessage().build();
        int profileIndex = 0;
        Set<String> profileNames = new HashSet<String>(profiles.getProfilesCount());
        for(DisplayProfile profile : profiles.getProfilesList()) {
            if(profile.getName().isEmpty()) {
                Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Profile #" + profileIndex + " has no name.\nA profile must have a uniqe name", null);
                StatusManager.getManager().handle(status, StatusManager.SHOW);
                return false;
            }
            if(profile.getQualifiersCount() == 0) {
                Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Profile '" + profile.getName() + "' has no qualifiers.\nAt least one qualifier per profile is required", null);
                StatusManager.getManager().handle(status, StatusManager.SHOW);
                return false;
            }
            if(profileNames.contains(profile.getName())) {
                Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Profile '" + profile.getName() + "' is not unique.\nA profile must have a uniqe name", null);
                StatusManager.getManager().handle(status, StatusManager.SHOW);
                return false;
            }
            if(profile.getName().toLowerCase().compareTo("default")==0) {
                Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Profile '" + profile.getName() + "' invalid, using reserved name 'Default'.", null);
                StatusManager.getManager().handle(status, StatusManager.SHOW);
                return false;
            }
            profileNames.add(profile.getName());
            profileIndex++;
        }
        return true;
    }

    @Override
    public void doSave(IProgressMonitor monitor) {
        if(!validateProfiles()) {
            return;
        }
        super.doSave(monitor);
    }

    @Override
    public void doSaveAs() {
        if(!validateProfiles()) {
            return;
        }
        super.doSaveAs();
    }

}
