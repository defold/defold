package com.dynamo.cr.web2.client.ui;

import com.google.gwt.user.client.DOM;
import com.google.gwt.user.client.Element;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.SimplePanel;

public class GoodbyeViewImpl extends Composite implements GoodbyeView
{

	SimplePanel viewPanel = new SimplePanel();
	Element nameSpan = DOM.createSpan();

	public GoodbyeViewImpl()
	{
		viewPanel.getElement().appendChild(nameSpan);
		initWidget(viewPanel);
	}

	@Override
	public void setName(String name)
	{
		nameSpan.setInnerText("Good-bye, " + name);
	}

}
