/*
 * Copyright (c) 2002-2010 Gargoyle Software Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.gargoylesoftware.htmlunit.html;

import com.gargoylesoftware.htmlunit.Page;
import com.gargoylesoftware.htmlunit.SgmlPage;
import com.gargoylesoftware.htmlunit.WebClient;
import com.gargoylesoftware.htmlunit.WebRequest;
import com.gargoylesoftware.htmlunit.WebResponse;
import com.gargoylesoftware.htmlunit.javascript.PostponedAction;
import com.gargoylesoftware.htmlunit.javascript.host.Event;
import com.gargoylesoftware.htmlunit.javascript.host.Node;

import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.util.Iterator;
import java.util.Map;

import javax.imageio.ImageIO;
import javax.imageio.ImageReader;
import javax.imageio.stream.ImageInputStream;

/**
 * CUSTOM HTMLIMAGE IMPLEMENTATION THAT AVOIDS ANY IMAGES DOWNLOAD.
 */
public class HtmlImage extends HtmlElement {

    /** The HTML tag represented by this element. */
    public static final String TAG_NAME = "img";

    private int lastClickX_;
    private int lastClickY_;
    private WebResponse imageWebResponse_;
    private ImageReader imageReader_;
    private boolean downloaded_;
    private boolean onloadInvoked_;

    /**
     * Creates a new instance.
     * @param namespaceURI the URI that identifies an XML namespace
     * @param qualifiedName the qualified name of the element type to instantiate
     * @param page the page that contains this element
     * @param attributes the initial attributes
     */
    HtmlImage(final String namespaceURI, final String qualifiedName, final SgmlPage page, final Map<String, DomAttr> attributes) {
        super(namespaceURI, qualifiedName, page, attributes);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    protected void onAddedToPage() {
        doOnLoad();
        super.onAddedToPage();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setAttributeNS(final String namespaceURI, final String qualifiedName, final String value) {
        if ("src".equals(qualifiedName) && value != ATTRIBUTE_NOT_DEFINED && getPage() instanceof HtmlPage) {
            final String oldValue = getAttributeNS(namespaceURI, qualifiedName);
            if (!oldValue.equals(value)) {
                // onload handlers may need to be invoked again, and a new image may need to be downloaded
                onloadInvoked_ = false;
                downloaded_ = false;
            }
        }
        super.setAttributeNS(namespaceURI, qualifiedName, value);
    }

    /**
     * <p>
     * <span style="color:red">INTERNAL API - SUBJECT TO CHANGE AT ANY TIME - USE AT YOUR OWN RISK.</span>
     * </p>
     * <p>
     * Executes this element's <tt>onload</tt> handler if it has one. This method also downloads the image if this
     * element has an <tt>onload</tt> handler (prior to invoking said handler), because applications sometimes use
     * images to send information to the server and use the <tt>onload</tt> handler to get notified when the information
     * has been received by the server.
     * </p>
     * <p>
     * See <a href="http://www.nabble.com/How-should-we-handle-image.onload--tt9850876.html">here</a> and <a
     * href="http://www.nabble.com/Image-Onload-Support-td18895781.html">here</a> for the discussion which lead up to
     * this method.
     * </p>
     * <p>
     * This method may be called multiple times, but will only attempt to execute the <tt>onload</tt> handler the first
     * time it is invoked.
     * </p>
     */
    public void doOnLoad() {
        if (!(getPage() instanceof HtmlPage)) {
            return; // nothing to do if embedded in XML code
        } else if (onloadInvoked_) {
            return;
        }
        onloadInvoked_ = true;
        final HtmlPage htmlPage = (HtmlPage) getPage();
        if (hasEventHandlers("onload")) {
            // DO NOT DOWNLOAD MY IMAGES!!!!
            final Event event = new Event(this, Event.TYPE_LOAD);
            final Node scriptObject = (Node) getScriptObject();
            final PostponedAction action = new PostponedAction(getPage()) {
                @Override
                public void execute() throws Exception {
                    scriptObject.executeEvent(event);
                }
            };
            final String readyState = htmlPage.getReadyState();
            if (READY_STATE_LOADING.equals(readyState)) {
                htmlPage.addAfterLoadAction(action);
            } else {
                htmlPage.getWebClient().getJavaScriptEngine().addPostponedAction(action);
            }
        }
    }

    /**
     * Returns the value of the attribute "src". Refer to the <a href='http://www.w3.org/TR/html401/'>HTML 4.01</a>
     * documentation for details on the use of this attribute.
     * @return the value of the attribute "src" or an empty string if that attribute isn't defined
     */
    public final String getSrcAttribute() {
        return getAttribute("src");
    }

    /**
     * Returns the value of the attribute "alt". Refer to the <a href='http://www.w3.org/TR/html401/'>HTML 4.01</a>
     * documentation for details on the use of this attribute.
     * @return the value of the attribute "alt" or an empty string if that attribute isn't defined
     */
    public final String getAltAttribute() {
        return getAttribute("alt");
    }

    /**
     * Returns the value of the attribute "name". Refer to the <a href='http://www.w3.org/TR/html401/'>HTML 4.01</a>
     * documentation for details on the use of this attribute.
     * @return the value of the attribute "name" or an empty string if that attribute isn't defined
     */
    public final String getNameAttribute() {
        return getAttribute("name");
    }

    /**
     * Returns the value of the attribute "longdesc". Refer to the <a href='http://www.w3.org/TR/html401/'>HTML 4.01</a>
     * documentation for details on the use of this attribute.
     * @return the value of the attribute "longdesc" or an empty string if that attribute isn't defined
     */
    public final String getLongDescAttribute() {
        return getAttribute("longdesc");
    }

    /**
     * Returns the value of the attribute "height". Refer to the <a href='http://www.w3.org/TR/html401/'>HTML 4.01</a>
     * documentation for details on the use of this attribute.
     * @return the value of the attribute "height" or an empty string if that attribute isn't defined
     */
    public final String getHeightAttribute() {
        return getAttribute("height");
    }

    /**
     * Returns the value of the attribute "width". Refer to the <a href='http://www.w3.org/TR/html401/'>HTML 4.01</a>
     * documentation for details on the use of this attribute.
     * @return the value of the attribute "width" or an empty string if that attribute isn't defined
     */
    public final String getWidthAttribute() {
        return getAttribute("width");
    }

    /**
     * Returns the value of the attribute "usemap". Refer to the <a href='http://www.w3.org/TR/html401/'>HTML 4.01</a>
     * documentation for details on the use of this attribute.
     * @return the value of the attribute "usemap" or an empty string if that attribute isn't defined
     */
    public final String getUseMapAttribute() {
        return getAttribute("usemap");
    }

    /**
     * Returns the value of the attribute "ismap". Refer to the <a href='http://www.w3.org/TR/html401/'>HTML 4.01</a>
     * documentation for details on the use of this attribute.
     * @return the value of the attribute "ismap" or an empty string if that attribute isn't defined
     */
    public final String getIsmapAttribute() {
        return getAttribute("ismap");
    }

    /**
     * Returns the value of the attribute "align". Refer to the <a href='http://www.w3.org/TR/html401/'>HTML 4.01</a>
     * documentation for details on the use of this attribute.
     * @return the value of the attribute "align" or an empty string if that attribute isn't defined
     */
    public final String getAlignAttribute() {
        return getAttribute("align");
    }

    /**
     * Returns the value of the attribute "border". Refer to the <a href='http://www.w3.org/TR/html401/'>HTML 4.01</a>
     * documentation for details on the use of this attribute.
     * @return the value of the attribute "border" or an empty string if that attribute isn't defined
     */
    public final String getBorderAttribute() {
        return getAttribute("border");
    }

    /**
     * Returns the value of the attribute "hspace". Refer to the <a href='http://www.w3.org/TR/html401/'>HTML 4.01</a>
     * documentation for details on the use of this attribute.
     * @return the value of the attribute "hspace" or an empty string if that attribute isn't defined
     */
    public final String getHspaceAttribute() {
        return getAttribute("hspace");
    }

    /**
     * Returns the value of the attribute "vspace". Refer to the <a href='http://www.w3.org/TR/html401/'>HTML 4.01</a>
     * documentation for details on the use of this attribute.
     * @return the value of the attribute "vspace" or an empty string if that attribute isn't defined
     */
    public final String getVspaceAttribute() {
        return getAttribute("vspace");
    }

    /**
     * <p>
     * Returns the image's actual height (<b>not</b> the image's {@link #getHeightAttribute() height attribute}).
     * </p>
     * <p>
     * <span style="color:red">POTENTIAL PERFORMANCE KILLER - DOWNLOADS THE IMAGE - USE AT YOUR OWN RISK</span>
     * </p>
     * <p>
     * If the image has not already been downloaded, this method triggers a download and caches the image.
     * </p>
     * @return the image's actual height
     * @throws IOException if an error occurs while downloading or reading the image
     */
    public int getHeight() throws IOException {
        readImageIfNeeded();
        return imageReader_.getHeight(0);
    }

    /**
     * <p>
     * Returns the image's actual width (<b>not</b> the image's {@link #getWidthAttribute() width attribute}).
     * </p>
     * <p>
     * <span style="color:red">POTENTIAL PERFORMANCE KILLER - DOWNLOADS THE IMAGE - USE AT YOUR OWN RISK</span>
     * </p>
     * <p>
     * If the image has not already been downloaded, this method triggers a download and caches the image.
     * </p>
     * @return the image's actual width
     * @throws IOException if an error occurs while downloading or reading the image
     */
    public int getWidth() throws IOException {
        readImageIfNeeded();
        return imageReader_.getWidth(0);
    }

    /**
     * <p>
     * Returns the <tt>ImageReader</tt> which can be used to read the image contained by this image element.
     * </p>
     * <p>
     * <span style="color:red">POTENTIAL PERFORMANCE KILLER - DOWNLOADS THE IMAGE - USE AT YOUR OWN RISK</span>
     * </p>
     * <p>
     * If the image has not already been downloaded, this method triggers a download and caches the image.
     * </p>
     * @return the <tt>ImageReader</tt> which can be used to read the image contained by this image element
     * @throws IOException if an error occurs while downloading or reading the image
     */
    public ImageReader getImageReader() throws IOException {
        readImageIfNeeded();
        return imageReader_;
    }

    /**
     * <p>
     * Returns the <tt>WebResponse</tt> for the image contained by this image element.
     * </p>
     * <p>
     * <span style="color:red">POTENTIAL PERFORMANCE KILLER - DOWNLOADS THE IMAGE - USE AT YOUR OWN RISK</span>
     * </p>
     * <p>
     * If the image has not already been downloaded and <tt>downloadIfNeeded</tt> is <tt>true</tt>, this method triggers
     * a download and caches the image.
     * </p>
     * @param downloadIfNeeded whether or not the image should be downloaded (if it hasn't already been downloaded)
     * @return <tt>null</tt> if no download should be performed and one hasn't already been triggered; otherwise, the
     *         response received when performing a request for the image referenced by this element
     * @throws IOException if an error occurs while downloading the image
     */
    public WebResponse getWebResponse(final boolean downloadIfNeeded) throws IOException {
        if (downloadIfNeeded) {
            downloadImageIfNeeded();
        }
        return imageWebResponse_;
    }

    /**
     * <p>
     * Downloads the image contained by this image element.
     * </p>
     * <p>
     * <span style="color:red">POTENTIAL PERFORMANCE KILLER - DOWNLOADS THE IMAGE - USE AT YOUR OWN RISK</span>
     * </p>
     * <p>
     * If the image has not already been downloaded, this method triggers a download and caches the image.
     * </p>
     * @throws IOException if an error occurs while downloading the image
     */
    private void downloadImageIfNeeded() throws IOException {
        if (!downloaded_) {
            final HtmlPage page = (HtmlPage) getPage();
            final WebClient webclient = page.getWebClient();

            final URL url = page.getFullyQualifiedUrl(getSrcAttribute());
            final WebRequest request = new WebRequest(url);
            request.setAdditionalHeader("Referer", page.getWebResponse().getWebRequest().getUrl().toExternalForm());
            imageWebResponse_ = webclient.loadWebResponse(request);
            downloaded_ = true;
        }
    }

    private void readImageIfNeeded() throws IOException {
        downloadImageIfNeeded();
        if (imageReader_ == null) {
            final ImageInputStream iis = ImageIO.createImageInputStream(imageWebResponse_.getContentAsStream());
            final Iterator<ImageReader> iter = ImageIO.getImageReaders(iis);
            if (!iter.hasNext()) {
                throw new IOException("No image detected in response");
            }
            imageReader_ = iter.next();
            imageReader_.setInput(iis);
        }
    }

    /**
     * Simulates clicking this element at the specified position. This only makes sense for an image map (currently only
     * server side), where the position matters. This method returns the page contained by this image's window after the
     * click, which may or may not be the same as the original page, depending on JavaScript event handlers, etc.
     * @param x the x position of the click
     * @param y the y position of the click
     * @return the page contained by this image's window after the click
     * @exception IOException if an IO error occurs
     */
    public Page click(final int x, final int y) throws IOException {
        lastClickX_ = x;
        lastClickY_ = y;
        return super.click();
    }

    /**
     * Simulates clicking this element at the position <tt>(0, 0)</tt>. This method returns the page contained by this
     * image's window after the click, which may or may not be the same as the original page, depending on JavaScript
     * event handlers, etc.
     * @return the page contained by this image's window after the click
     * @exception IOException if an IO error occurs
     */
    @Override
    @SuppressWarnings("unchecked")
    public Page click() throws IOException {
        return click(0, 0);
    }

    /**
     * Performs the click action on the enclosing A tag (if any).
     * @throws IOException if an IO error occurred
     */
    @Override
    protected void doClickAction() throws IOException {
        if (getUseMapAttribute() != ATTRIBUTE_NOT_DEFINED) {
            // remove initial '#'
            final String mapName = getUseMapAttribute().substring(1);
            final HtmlElement doc = ((HtmlPage) getPage()).getDocumentElement();
            final HtmlMap map = doc.getOneHtmlElementByAttribute("map", "name", mapName);
            for (final HtmlElement element : map.getChildElements()) {
                if (element instanceof HtmlArea) {
                    final HtmlArea area = (HtmlArea) element;
                    if (area.containsPoint(lastClickX_, lastClickY_)) {
                        area.doClickAction();
                        return;
                    }
                }
            }
        }
        final HtmlAnchor anchor = (HtmlAnchor) getEnclosingElement("a");
        if (anchor == null) {
            return;
        }
        if (getIsmapAttribute() != ATTRIBUTE_NOT_DEFINED) {
            final String suffix = "?" + lastClickX_ + "," + lastClickY_;
            anchor.doClickAction(suffix);
            return;
        }
        anchor.doClickAction();
    }

    /**
     * Saves this image as the specified file.
     * @param file the file to save to
     * @throws IOException if an IO error occurs
     */
    public void saveAs(final File file) throws IOException {
        final ImageReader reader = getImageReader();
        ImageIO.write(reader.read(0), reader.getFormatName(), file);
    }
}
