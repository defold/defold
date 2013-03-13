package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.Path;
import org.junit.Test;

import com.dynamo.cr.go.core.CollectionInstanceNode;
import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.CollectionPropertyNode;
import com.dynamo.cr.go.core.ComponentPropertyNode;
import com.dynamo.cr.go.core.GameObjectInstanceNode;
import com.dynamo.cr.go.core.GameObjectPropertyNode;

public class ScriptPropertiesCollectionTest extends AbstractSceneTest {

    @Override
    public void setup() throws CoreException, IOException {
        super.setup();

        getPresenter().onLoad("collection", new ByteArrayInputStream("name: \"main\" instances { id: \"go\" prototype: \"/game_object/props.go\"}".getBytes()));
    }

    private void saveFile(String path, String content) throws IOException, CoreException {
        IFile file = getContentRoot().getFile(new Path(path));
        InputStream stream = new ByteArrayInputStream(content.getBytes());
        if (!file.exists()) {
            file.create(stream, true, null);
        } else {
            file.setContents(stream, 0, null);
        }
    }

    // Tests

    @Test
    public void testAccess() throws Exception {

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        GameObjectInstanceNode gameObject = (GameObjectInstanceNode)collection.getChildren().get(0);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(1);

        // Default value
        assertEquals(2.0, getNodeProperty(component, "number"));
        assertEquals("hash2", getNodeProperty(component, "hash"));
        assertEquals("/url", getNodeProperty(component, "url"));

        // Set value
        setNodeProperty(component, "number", 3.0);
        assertEquals(3.0, getNodeProperty(component, "number"));
        setNodeProperty(component, "hash", "hash3");
        assertEquals("hash3", getNodeProperty(component, "hash"));
        setNodeProperty(component, "url", "/url2");
        assertEquals("/url2", getNodeProperty(component, "url"));

        // Reset to default
        resetNodeProperty(component, "number");
        assertEquals(2.0, getNodeProperty(component, "number"));
        resetNodeProperty(component, "hash");
        assertEquals("hash2", getNodeProperty(component, "hash"));
        resetNodeProperty(component, "url");
        assertEquals("/url", getNodeProperty(component, "url"));

    }

    @Test
    public void testLoad() throws Exception {
        getPresenter().onLoad("collection", ((IFile)getContentRoot().findMember("/collection/props.collection")).getContents());

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        GameObjectInstanceNode gameObject = (GameObjectInstanceNode)collection.getChildren().get(0);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(1);

        assertEquals(3.0, getNodeProperty(component, "number"));
        assertEquals("hash3", getNodeProperty(component, "hash"));
        assertEquals("/url2", getNodeProperty(component, "url"));
    }

    @Test
    public void testLoadSub() throws Exception {
        getPresenter().onLoad("collection", ((IFile)getContentRoot().findMember("/collection/sub_props.collection")).getContents());

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        CollectionInstanceNode collectionInstance = (CollectionInstanceNode)collection.getChildren().get(0);
        GameObjectPropertyNode gameObject = (GameObjectPropertyNode)collectionInstance.getChildren().get(1);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(0);

        assertEquals(4.0, getNodeProperty(component, "number"));
        assertEquals("hash4", getNodeProperty(component, "hash"));
        assertEquals("/url3", getNodeProperty(component, "url"));
    }

    @Test
    public void testLoadSubSub() throws Exception {
        getPresenter().onLoad("collection", ((IFile)getContentRoot().findMember("/collection/sub_sub_props.collection")).getContents());

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        CollectionInstanceNode collectionInstance = (CollectionInstanceNode)collection.getChildren().get(0);
        CollectionPropertyNode collectionProperty = (CollectionPropertyNode)collectionInstance.getChildren().get(1);
        GameObjectPropertyNode gameObject = (GameObjectPropertyNode)collectionProperty.getChildren().get(0);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(0);

        assertEquals(5.0, getNodeProperty(component, "number"));
        assertEquals("hash5", getNodeProperty(component, "hash"));
        assertEquals("/url4", getNodeProperty(component, "url"));
    }

    @Test
    public void testLoadSubEmbed() throws Exception {
        getPresenter().onLoad("collection", ((IFile)getContentRoot().findMember("/collection/sub_embed.collection")).getContents());

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        CollectionInstanceNode collectionInstance = (CollectionInstanceNode)collection.getChildren().get(0);
        GameObjectPropertyNode gameObject = (GameObjectPropertyNode)collectionInstance.getChildren().get(1);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(0);

        assertEquals(3.0, getNodeProperty(component, "number"));
        assertEquals("hash3", getNodeProperty(component, "hash"));
        assertEquals("/url2", getNodeProperty(component, "url"));
    }

    @Test
    public void testLoadSubDefaults() throws Exception {
        getPresenter().onLoad("collection", ((IFile)getContentRoot().findMember("/collection/sub_defaults.collection")).getContents());

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        CollectionInstanceNode collectionInstance = (CollectionInstanceNode)collection.getChildren().get(0);
        GameObjectPropertyNode gameObject = (GameObjectPropertyNode)collectionInstance.getChildren().get(1);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(0);

        assertEquals(3.0, getNodeProperty(component, "number"));
        assertEquals("hash3", getNodeProperty(component, "hash"));
        assertEquals("/url2", getNodeProperty(component, "url"));
        assertFalse(isNodePropertyOverridden(component, "number"));
        assertFalse(isNodePropertyOverridden(component, "hash"));
        assertFalse(isNodePropertyOverridden(component, "url"));
    }

    @Test
    public void testLoadSubSubDefaults() throws Exception {
        getPresenter().onLoad("collection", ((IFile)getContentRoot().findMember("/collection/sub_sub_defaults.collection")).getContents());

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        CollectionInstanceNode collectionInstance = (CollectionInstanceNode)collection.getChildren().get(0);
        CollectionPropertyNode collectionProperty = (CollectionPropertyNode)collectionInstance.getChildren().get(1);
        GameObjectPropertyNode gameObject = (GameObjectPropertyNode)collectionProperty.getChildren().get(0);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(0);

        assertEquals(4.0, getNodeProperty(component, "number"));
        assertEquals("hash4", getNodeProperty(component, "hash"));
        assertEquals("/url3", getNodeProperty(component, "url"));
        assertFalse(isNodePropertyOverridden(component, "number"));
        assertFalse(isNodePropertyOverridden(component, "hash"));
        assertFalse(isNodePropertyOverridden(component, "url"));
    }

    @Test
    public void testSave() throws Exception {
        IFile collectionFile = (IFile)getContentRoot().findMember("/collection/props.collection");
        getPresenter().onLoad("collection", collectionFile.getContents());

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        GameObjectInstanceNode gameObject = (GameObjectInstanceNode)collection.getChildren().get(0);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(1);

        setNodeProperty(component, "number", "4");
        setNodeProperty(component, "hash", "hash4");
        setNodeProperty(component, "url", "/url3");

        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        getPresenter().onSave(stream, null);
        collectionFile.setContents(new ByteArrayInputStream(stream.toByteArray()), false, true, null);

        getPresenter().onLoad("collection", collectionFile.getContents());
        collection = (CollectionNode)getModel().getRoot();
        gameObject = (GameObjectInstanceNode)collection.getChildren().get(0);
        component = (ComponentPropertyNode)gameObject.getChildren().get(1);

        assertEquals(4.0, getNodeProperty(component, "number"));
        assertEquals("hash4", getNodeProperty(component, "hash"));
        assertEquals("/url3", getNodeProperty(component, "url"));
    }

    @Test
    public void testSaveSub() throws Exception {
        IFile collectionFile = (IFile)getContentRoot().findMember("/collection/sub_props.collection");
        getPresenter().onLoad("collection", collectionFile.getContents());

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        CollectionInstanceNode collectionInstance = (CollectionInstanceNode)collection.getChildren().get(0);
        GameObjectPropertyNode gameObject = (GameObjectPropertyNode)collectionInstance.getChildren().get(1);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(0);

        setNodeProperty(component, "number", "5");
        setNodeProperty(component, "hash", "hash5");
        setNodeProperty(component, "url", "/url4");

        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        getPresenter().onSave(stream, null);
        collectionFile.setContents(new ByteArrayInputStream(stream.toByteArray()), false, true, null);

        getPresenter().onLoad("collection", collectionFile.getContents());
        collection = (CollectionNode)getModel().getRoot();
        collectionInstance = (CollectionInstanceNode)collection.getChildren().get(0);
        gameObject = (GameObjectPropertyNode)collectionInstance.getChildren().get(1);
        component = (ComponentPropertyNode)gameObject.getChildren().get(0);

        assertEquals(5.0, getNodeProperty(component, "number"));
        assertEquals("hash5", getNodeProperty(component, "hash"));
        assertEquals("/url4", getNodeProperty(component, "url"));
    }

    @Test
    public void testSaveSubSub() throws Exception {
        IFile collectionFile = (IFile)getContentRoot().findMember("/collection/sub_sub_props.collection");
        getPresenter().onLoad("collection", collectionFile.getContents());

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        CollectionInstanceNode collectionInstance = (CollectionInstanceNode)collection.getChildren().get(0);
        CollectionPropertyNode collectionProperty = (CollectionPropertyNode)collectionInstance.getChildren().get(1);
        GameObjectPropertyNode gameObject = (GameObjectPropertyNode)collectionProperty.getChildren().get(0);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(0);

        setNodeProperty(component, "number", "6");
        setNodeProperty(component, "hash", "hash6");
        setNodeProperty(component, "url", "/url5");

        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        getPresenter().onSave(stream, null);
        collectionFile.setContents(new ByteArrayInputStream(stream.toByteArray()), false, true, null);

        getPresenter().onLoad("collection", collectionFile.getContents());
        collection = (CollectionNode)getModel().getRoot();
        collectionInstance = (CollectionInstanceNode)collection.getChildren().get(0);
        collectionProperty = (CollectionPropertyNode)collectionInstance.getChildren().get(1);
        gameObject = (GameObjectPropertyNode)collectionProperty.getChildren().get(0);
        component = (ComponentPropertyNode)gameObject.getChildren().get(0);

        assertEquals(6.0, getNodeProperty(component, "number"));
        assertEquals("hash6", getNodeProperty(component, "hash"));
        assertEquals("/url5", getNodeProperty(component, "url"));
    }

    @Test(expected = RuntimeException.class)
    public void testReload() throws Exception {
        CollectionNode collection = (CollectionNode)getModel().getRoot();
        GameObjectInstanceNode gameObject = (GameObjectInstanceNode)collection.getChildren().get(0);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(1);

        // Default value
        assertEquals(2.0, getNodeProperty(component, "number"));

        saveFile("/script/props.script", "go.property(\"number2\", 0)");

        getNodeProperty(component, "number");
    }

    @Test(expected = RuntimeException.class)
    public void testReloadSub() throws Exception {
        IFile collectionFile = (IFile)getContentRoot().findMember("/collection/sub_props.collection");
        getPresenter().onLoad("collection", collectionFile.getContents());

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        CollectionInstanceNode collectionInstance = (CollectionInstanceNode)collection.getChildren().get(0);
        GameObjectPropertyNode gameObject = (GameObjectPropertyNode)collectionInstance.getChildren().get(1);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(0);

        assertEquals(4.0, getNodeProperty(component, "number"));

        saveFile("/script/props.script", "go.property(\"number2\", 0)");

        getNodeProperty(component, "number");
    }

    @Test
    public void testReloadSubStructure() throws Exception {
        IFile collectionFile = (IFile)getContentRoot().findMember("/collection/sub_props.collection");
        getPresenter().onLoad("collection", collectionFile.getContents());

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        CollectionInstanceNode collectionInstance = (CollectionInstanceNode)collection.getChildren().get(0);
        GameObjectPropertyNode gameObject = (GameObjectPropertyNode)collectionInstance.getChildren().get(1);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(0);

        assertEquals(4.0, getNodeProperty(component, "number"));

        saveFile("/game_object/props.go", "");

        collectionInstance = (CollectionInstanceNode)collection.getChildren().get(0);
        gameObject = (GameObjectPropertyNode)collectionInstance.getChildren().get(1);
        assertTrue(gameObject.getChildren().isEmpty());
    }

    @Test(expected = RuntimeException.class)
    public void testReloadSubSub() throws Exception {
        IFile collectionFile = (IFile)getContentRoot().findMember("/collection/sub_sub_props.collection");
        getPresenter().onLoad("collection", collectionFile.getContents());

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        CollectionInstanceNode collectionInstance = (CollectionInstanceNode)collection.getChildren().get(0);
        CollectionPropertyNode collectionProperty = (CollectionPropertyNode)collectionInstance.getChildren().get(1);
        GameObjectPropertyNode gameObject = (GameObjectPropertyNode)collectionProperty.getChildren().get(0);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(0);

        assertEquals(5.0, getNodeProperty(component, "number"));

        saveFile("/script/props.script", "go.property(\"number2\", 0)");

        getNodeProperty(component, "number");
    }

    @Test
    public void testSavePropEqualToDefault() throws Exception {
        getPresenter().onLoad("collection", ((IFile)getContentRoot().findMember("/collection/empty_props_go.collection")).getContents());

        CollectionNode collection = (CollectionNode)getModel().getRoot();
        GameObjectInstanceNode gameObject = (GameObjectInstanceNode)collection.getChildren().get(0);
        ComponentPropertyNode component = (ComponentPropertyNode)gameObject.getChildren().get(1);

        assertEquals(3.0, getNodeProperty(component, "number"));

        // Set to default
        setNodeProperty(component, "number", "1");

        // Save file
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        getPresenter().onSave(stream, null);
        IFile collectionFile = (IFile)getContentRoot().findMember("/collection/empty_props_go.collection");
        collectionFile.setContents(new ByteArrayInputStream(stream.toByteArray()), false, true, null);

        // Change script to new value
        saveFile("/script/props.script", "go.property(\"number\", 3)");

        // Load file again
        getPresenter().onLoad("collection", ((IFile)getContentRoot().findMember("/collection/empty_props_go.collection")).getContents());

        collection = (CollectionNode)getModel().getRoot();
        gameObject = (GameObjectInstanceNode)collection.getChildren().get(0);
        component = (ComponentPropertyNode)gameObject.getChildren().get(1);

        // Verify new value
        assertEquals(1.0, getNodeProperty(component, "number"));
    }
}
