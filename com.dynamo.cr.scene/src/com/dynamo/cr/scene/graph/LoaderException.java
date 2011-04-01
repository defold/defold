package com.dynamo.cr.scene.graph;


public class LoaderException extends Exception
{
    public LoaderException(String message)
    {
        super(message);
    }

    public LoaderException(String message, Throwable e)
    {
        super(message, e);
    }

    public LoaderException(Throwable e) {
        super(e);
    }

    private static final long serialVersionUID = 8396408423837023974L;

}
