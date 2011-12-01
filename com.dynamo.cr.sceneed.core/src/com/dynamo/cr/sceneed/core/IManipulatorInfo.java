package com.dynamo.cr.sceneed.core;


public interface IManipulatorInfo {

    IManipulatorFactory getFactory();

    IManipulatorMode getMode();

    String getName();

}