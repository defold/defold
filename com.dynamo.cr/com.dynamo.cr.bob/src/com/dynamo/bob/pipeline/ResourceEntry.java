package com.dynamo.bob.pipeline;

public class ResourceEntry {
    public String resourceAbsPath;
    public String parentRelPath;
    
    public ResourceEntry(String resourceAbsPath, String parentRelPath) {
        this.resourceAbsPath = resourceAbsPath; 
        this.parentRelPath = parentRelPath;
    }

    @Override
    public boolean equals(Object other){
        boolean result = this.getClass().equals(other.getClass());
         if (result) {
             ResourceEntry resourceOther = (ResourceEntry)other;
             result = this.resourceAbsPath.equals(resourceOther.resourceAbsPath) && this.parentRelPath.equals(resourceOther.parentRelPath);
         }
        return result;
    }
    
    public int hashCode()
    {
        return 17 * this.resourceAbsPath.hashCode() + 31 * this.parentRelPath.hashCode();
    }
}