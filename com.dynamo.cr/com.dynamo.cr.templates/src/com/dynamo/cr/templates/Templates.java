package com.dynamo.cr.templates;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.URL;
import java.util.Enumeration;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.runtime.FileLocator;
import org.eclipse.core.runtime.Plugin;
import org.osgi.framework.Bundle;
import org.osgi.framework.BundleContext;

public class Templates extends Plugin {

    private static Templates plugin;

    public static Templates getDefault() {
        return plugin;
    }

    static private void extractFolder(String zipFile, String newPath) throws ZipException, IOException
    {
        File file = new File(zipFile);
        if (!file.exists()) {
            Logger logger = Logger.getLogger(Templates.class.getPackage().getName());
            logger.log(Level.WARNING, String.format("Did not extract %s since the file is missing.", zipFile));
            return;
        }

        ZipFile zip = new ZipFile(file);
        new File(newPath).mkdir();
        Enumeration<? extends ZipEntry> zipFileEntries = zip.entries();

        while (zipFileEntries.hasMoreElements())
        {
            ZipEntry entry = (ZipEntry) zipFileEntries.nextElement();
            String currentEntry = entry.getName();
            File destFile = new File(newPath, currentEntry);
            File destinationParent = destFile.getParentFile();
            destinationParent.mkdirs();
            if (!entry.isDirectory())
            {
                BufferedInputStream is = new BufferedInputStream(zip.getInputStream(entry));
                FileOutputStream fos = new FileOutputStream(destFile);
                IOUtils.copy(is, fos);
                fos.flush();
                fos.close();
                is.close();
            }
        }

        zip.close();
    }

	public void start(BundleContext bundleContext) throws Exception {
        super.start(bundleContext);

        String zipFile = String.format("%s/templates.zip", getTemplateRoot());
        extractFolder(zipFile, getTemplateRoot());

		plugin = this;
	}

	public void stop(BundleContext bundleContext) throws Exception {
        super.stop(bundleContext);
		plugin = null;
	}

    public String getTemplateRoot() {
        Bundle bundle = getBundle();
        URL bundleUrl = bundle.getEntry("/templates");
        URL fileUrl;
        try {
            fileUrl = FileLocator.toFileURL(bundleUrl);
            return fileUrl.getPath();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}
