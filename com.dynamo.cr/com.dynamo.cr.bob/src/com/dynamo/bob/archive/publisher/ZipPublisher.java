package com.dynamo.bob.archive.publisher;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.apache.commons.io.IOUtils;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;

public class ZipPublisher extends Publisher {

    private File resourcePackZip = null;

    public ZipPublisher(PublisherSettings settings) {
        super(settings);
    }

    @Override
    public void Publish() throws CompileExceptionError {
    	try {
            String tempFilePrefix = "defold.resourcepack_" + this.platform + "_";
	    	this.resourcePackZip = File.createTempFile(tempFilePrefix, ".zip");
	        FileOutputStream resourcePackOutputStream = new FileOutputStream(this.resourcePackZip);
	        ZipOutputStream zipOutputStream = new ZipOutputStream(resourcePackOutputStream);
	        try {
	            for (String hexDigest : this.getEntries().keySet()) {
		            File fhandle = this.getEntries().get(hexDigest);
		            ZipEntry currentEntry = new ZipEntry(fhandle.getName());
		            zipOutputStream.putNextEntry(currentEntry);
	
		            FileInputStream currentInputStream = new FileInputStream(fhandle);
		            int currentLength = 0;
		            byte[] currentBuffer = new byte[1024];
		            while ((currentLength = currentInputStream.read(currentBuffer)) > 0) {
		                zipOutputStream.write(currentBuffer, 0, currentLength);
		            }
		            
		        	zipOutputStream.closeEntry();
		            IOUtils.closeQuietly(currentInputStream);
	            }
	        } catch (FileNotFoundException exception) {
	        	throw new CompileExceptionError("Unable to find required file for liveupdate resources: " + exception.getMessage(), exception);
	        } catch (IOException exception) {
	        	throw new CompileExceptionError("Unable to write to zip archive for liveupdate resources: " + exception.getMessage(), exception);
	        } finally {
	            IOUtils.closeQuietly(zipOutputStream);
	        }
	        
	        File exportFilehandle = new File(this.getPublisherSettings().getZipFilepath(), this.resourcePackZip.getName());
	        Files.move(this.resourcePackZip.toPath(), exportFilehandle.toPath(), StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
    	} catch (IOException exception) {
        	throw new CompileExceptionError("Unable to create zip archive for liveupdate resources: " + exception.getMessage(), exception);
    	}
    }

    public List<IResource> getOutputs(IResource input) {
        List<IResource> outputs = new ArrayList<IResource>();
        return outputs;
    }

    public List<InputStream> getOutputResults() {
        List<InputStream> outputs = new ArrayList<InputStream>();
        return outputs;
    }

}
