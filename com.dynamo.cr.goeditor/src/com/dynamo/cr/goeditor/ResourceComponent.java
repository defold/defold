package com.dynamo.cr.goeditor;

public class ResourceComponent extends Component {

    private String resource;

    public ResourceComponent(String resource) {
        this.resource = resource;
    }

    @Override
    public String getFileExtension() {
        int index = resource.lastIndexOf(".");
        return resource.substring(index + 1);
    }

    public String getResource() {
        return resource;
    }

}
