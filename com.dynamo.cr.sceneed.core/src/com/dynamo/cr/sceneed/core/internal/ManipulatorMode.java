package com.dynamo.cr.sceneed.core.internal;

import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.sceneed.core.IManipulatorInfo;
import com.dynamo.cr.sceneed.core.IManipulatorMode;

public class ManipulatorMode implements IManipulatorMode {

    private String id;
    private String name;
    private List<IManipulatorInfo> manipulatorInfos = new ArrayList<IManipulatorInfo>();

    public ManipulatorMode(String id, String name) {
        this.id = id;
        this.name = name;
    }

    public void addManipulatorInfo(IManipulatorInfo info) {
        this.manipulatorInfos.add(info);
    }

    @Override
    public String getId() {
        return id;
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public List<IManipulatorInfo> getManipulatorInfoList() {
        return manipulatorInfos;
    }

}
