package com.dynamo.cr.editor.handlers;

import java.io.BufferedInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;

import javax.ws.rs.core.UriBuilder;

import org.apache.commons.io.FilenameUtils;
import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IncrementalProjectBuilder;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.jobs.Job;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.handlers.HandlerUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.BobUtil;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.preferences.PreferenceConstants;
import com.dynamo.cr.target.core.ITarget;
import com.dynamo.cr.target.core.ITargetService;
import com.dynamo.cr.target.core.TargetPlugin;
import com.dynamo.resource.proto.Resource;
import com.dynamo.resource.proto.Resource.Reload;

public class ReloadResourceHandler extends AbstractHandler {

    private static Logger logger = LoggerFactory
            .getLogger(ReloadResourceHandler.class);

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {

        final IEditorPart editor = HandlerUtil.getActiveEditor(event);
        if (editor != null)
        {
            IEditorInput input = editor.getEditorInput();
            if (input instanceof IFileEditorInput) {
                final IFileEditorInput fileInput = (IFileEditorInput) input;
                IPath path = fileInput.getFile().getFullPath();

                IContainer contentRoot = EditorUtil.findContentRoot(fileInput.getFile());
                path = path.makeRelativeTo(contentRoot.getFullPath());

                String stringPath = path.toPortableString() + "c";
                // TODO Hack to handle tilesource => tilesetc (ProtoBuilders.java)
                // https://defold.fogbugz.com/default.asp?1770
                if (FilenameUtils.isExtension(stringPath, "tilesourcec")
                 || FilenameUtils.isExtension(stringPath, "atlasc")) {
                    stringPath = ResourceUtil.changeExt(stringPath, ".texturesetc");
                }
                final String sPath = stringPath;
                editor.getSite().getPage().saveEditor(editor, false);

                Job job = new Job("reload") {
                    @Override
                    protected IStatus run(IProgressMonitor monitor) {
                        try {
                            IProject project = fileInput.getFile().getProject();
                            HashMap<String, String> bobArgs = new HashMap<String, String>();
                            final IPreferenceStore store = Activator.getDefault().getPreferenceStore();
                            ITargetService targetService = TargetPlugin.getDefault().getTargetsService();
                            final boolean localBranch = store.getBoolean(PreferenceConstants.P_USE_LOCAL_BRANCHES);
                            if (localBranch)
                                bobArgs.put("location", "local");
                            else
                                bobArgs.put("location", "remote");

                            bobArgs.put("debug", "true");

                            HashMap<String, String> args = new HashMap<String, String>();
                            BobUtil.putBobArgs(bobArgs, args);
                            project.build(IncrementalProjectBuilder.INCREMENTAL_BUILD,  "com.dynamo.cr.editor.builders.contentbuilder", args, monitor);

                            Reload reload = Resource.Reload
                                    .newBuilder()
                                    .setResource(sPath)
                                    .build();

                            ITarget target = targetService.getSelectedTarget();
                            URL reloadUrl;
                            URL baseUrl = new URL("http://localhost:8001");
                            if (target.getUrl() != null) {
                                baseUrl = new URL(target.getUrl());
                            }
                            reloadUrl = UriBuilder.fromUri(baseUrl.toURI()).path("/post/@resource/reload").build().toURL();
                            HttpURLConnection c = (HttpURLConnection) reloadUrl.openConnection();
                            c.setDoOutput(true);
                            c.setRequestMethod("POST");
                            OutputStream os = c.getOutputStream();
                            os.write(reload.toByteArray());
                            os.close();

                            InputStream is = new BufferedInputStream(c.getInputStream());
                            int n = is.read();
                            while (n != -1)
                                n = is.read();
                            is.close();

                        } catch (Throwable e) {
                            logger.warn(e.getMessage());
                        }
                        return Status.OK_STATUS;
                    }
                };

                job.schedule();
            }
        }

        return null;
    }
}
