package com.dynamo.cr.integrationtest;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertThat;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.when;

import java.io.ByteArrayInputStream;
import java.io.CharArrayWriter;
import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.junit.Test;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.RefComponentNode;
import com.dynamo.cr.go.core.operations.AddComponentOperation;
import com.dynamo.cr.tileeditor.scene.Sprite2Node;
import com.dynamo.sprite2.proto.Sprite2.Sprite2Desc;
import com.google.protobuf.TextFormat;

public class GameObjectReloadTest extends AbstractSceneTest {

    @Override
    public void setup() throws CoreException, IOException {
        super.setup();

        getPresenter().onLoad("go", new ByteArrayInputStream("".getBytes()));
    }

    private void saveSprite2Component(String path, String tileSet, String defaultAnimation) throws IOException, CoreException {
        Sprite2Desc.Builder builder = Sprite2Desc.newBuilder();
        builder.setTileSet(tileSet).setDefaultAnimation(defaultAnimation);
        IFile file = getContentRoot().getFile(new Path(path));
        CharArrayWriter output = new CharArrayWriter();
        TextFormat.print(builder.build(), output);
        output.close();
        InputStream stream = new ByteArrayInputStream(output.toString().getBytes());
        if (!file.exists()) {
            file.create(stream, true, null);
        } else {
            file.setContents(stream, 0, null);
        }
    }

    // Tests

    @Test
    public void testReloadComponentFromFile() throws Exception {
        String path = "/sprite2/reload.sprite2";
        String tileSet = "/tileset/test.tileset";
        String defaultAnimation = "test";

        when(getPresenterContext().selectFile(anyString())).thenReturn(path);
        Sprite2Node componentType = new Sprite2Node();
        componentType.setTileSet(tileSet);
        componentType.setDefaultAnimation(defaultAnimation);

        saveSprite2Component(path, "", "");

        GameObjectNode go = (GameObjectNode)getModel().getRoot();
        RefComponentNode component = new RefComponentNode(new Sprite2Node());
        component.setComponent(path);
        AddComponentOperation op = new AddComponentOperation(go, component, getPresenterContext());
        getModel().executeOperation(op);
        assertThat(go.getChildren().size(), is(1));
        assertNodePropertyStatus(component, "component", IStatus.ERROR, null);
        ComponentTypeNode type = component.getType();

        saveSprite2Component(path, tileSet, defaultAnimation);

        assertNodePropertyStatus(component, "component", IStatus.OK, null);
        assertThat((RefComponentNode)go.getChildren().get(0), is(component));
        assertThat(type, is(not(component.getType())));
    }

}
