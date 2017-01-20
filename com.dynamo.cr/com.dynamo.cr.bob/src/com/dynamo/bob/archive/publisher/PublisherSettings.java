package com.dynamo.bob.archive.publisher;

public class PublisherSettings {

	public enum PublishMode {
		Amazon, Defold, Zip
	};
	
	public PublishMode mode = PublishMode.Zip;
	
	public String awsAccessKey = null;
	public String awsSecretKey = null;
	
	public String awsBucket = null;
	public String awsDirectory = null;
	
	public String zipFilepath = null;
	
}
