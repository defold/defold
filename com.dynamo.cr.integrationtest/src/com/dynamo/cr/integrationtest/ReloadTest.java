package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.StringWriter;
import java.net.URL;
import java.util.Enumeration;

import javax.imageio.ImageIO;
import javax.imageio.stream.MemoryCacheImageOutputStream;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IProjectDescription;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.junit.Before;
import org.junit.Test;
import org.osgi.framework.Bundle;

import com.dynamo.camera.proto.Camera.CameraDesc;
import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.cr.scene.resource.CameraLoader;
import com.dynamo.cr.scene.resource.CameraResource;
import com.dynamo.cr.scene.resource.CollisionLoader;
import com.dynamo.cr.scene.resource.CollisionResource;
import com.dynamo.cr.scene.resource.ConvexShapeLoader;
import com.dynamo.cr.scene.resource.ConvexShapeResource;
import com.dynamo.cr.scene.resource.LightLoader;
import com.dynamo.cr.scene.resource.LightResource;
import com.dynamo.cr.scene.resource.ResourceFactory;
import com.dynamo.cr.scene.resource.SpriteLoader;
import com.dynamo.cr.scene.resource.SpriteResource;
import com.dynamo.cr.scene.resource.TextureLoader;
import com.dynamo.cr.scene.resource.TextureResource;
import com.dynamo.gamesystem.proto.GameSystem.LightDesc;
import com.dynamo.gamesystem.proto.GameSystem.LightType;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.dynamo.physics.proto.Physics.CollisionObjectType;
import com.dynamo.physics.proto.Physics.ConvexShape;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.google.protobuf.TextFormat;

public class ReloadTest {

    private IProject project;
    private NullProgressMonitor monitor;
    private ResourceFactory factory;

    @Before
    public void setUp() throws Exception {
        project = ResourcesPlugin.getWorkspace().getRoot().getProject("test");
        monitor = new NullProgressMonitor();
        if (project.exists()) {
            project.delete(true, monitor);
        }
        project.create(monitor);
        project.open(monitor);

        IProjectDescription pd = project.getDescription();
        pd.setNatureIds(new String[] { "com.dynamo.cr.editor.core.crnature" });
        project.setDescription(pd, monitor);

        Bundle bundle = Platform.getBundle("com.dynamo.cr.integrationtest");
        @SuppressWarnings("unchecked")
        Enumeration<URL> entries = bundle.findEntries("/test", "*", true);
        while (entries.hasMoreElements()) {
            URL url = entries.nextElement();
            IPath path = new Path(url.getPath()).removeFirstSegments(1);
            // Create path of url-path and remove first element, ie /test/sounds/ -> /sounds
            if (url.getFile().endsWith("/")) {
                project.getFolder(path).create(true, true, monitor);
            } else {
                InputStream is = url.openStream();
                IFile file = project.getFile(path);
                file.create(is, true, monitor);
                is.close();
            }
        }
        factory = new ResourceFactory(project);
        factory.addLoader("png", new TextureLoader());
        factory.addLoader("camera", new CameraLoader());
        factory.addLoader("light", new LightLoader());
        factory.addLoader("sprite", new SpriteLoader());
        factory.addLoader("collisionobject", new CollisionLoader());
        factory.addLoader("convexshape", new ConvexShapeLoader());
        ResourcesPlugin.getWorkspace().addResourceChangeListener(factory);
    }

    @Test
    public void testSetup() {
        assertNotNull(project);
        assertNotNull(monitor);
        assertNotNull(factory);
    }

    private interface IMessageHelper<Resource, Message> {
        public Message getMessage(Resource resource);
        public void print(Message message, StringWriter writer) throws IOException;
    }

    private <Resource, Message> void testReload(String path, Resource resource, Message newMessage, IMessageHelper<Resource, Message> helper) throws CoreException, IOException {
        assertNotNull(resource);
        Message oldMessage = helper.getMessage(resource);
        assertFalse(newMessage.equals(oldMessage));
        StringWriter out = new StringWriter();
        helper.print(newMessage, out);
        InputStream in = new ByteArrayInputStream(out.toString().getBytes());
        out.close();
        IFile file = project.getFile(path);
        file.setContents(in, false, true, monitor);
        boolean equals = true;
        try {
            assertTrue(newMessage.equals(helper.getMessage(resource)));
        } catch (AssertionError e) {
            equals = false;
        } finally {
            out = new StringWriter();
            helper.print(oldMessage, out);
            in = new ByteArrayInputStream(out.toString().getBytes());
            file.setContents(in, false, true, monitor);
            out.close();
        }
        assertTrue(equals);
        assertFalse(newMessage.equals(helper.getMessage(resource)));
    }

    @Test
    public void testCamera() throws CoreException, CreateException, IOException {
        String path = "logic/test.camera";
        CameraResource resource = (CameraResource)factory.load(monitor, path);
        CameraDesc.Builder builder = CameraDesc.newBuilder();
        CameraDesc newMessage = builder.setAspectRatio(1.0f).setAutoAspectRatio(1).setFarZ(1.0f).setFov(1.0f).setNearZ(1.0f).build();
        testReload(path, resource, newMessage, new IMessageHelper<CameraResource, CameraDesc>() {
            public CameraDesc getMessage(CameraResource resource) {
                return resource.getCameraDesc();
            }
            public void print(CameraDesc message, StringWriter writer) throws IOException {
                TextFormat.print(message, writer);
            }
        });
    }

    @Test
    public void testCollision() throws CoreException, CreateException, IOException {
        String path = "logic/session/paddle.collisionobject";
        CollisionResource resource = (CollisionResource)factory.load(monitor, path);
        CollisionObjectDesc.Builder builder = CollisionObjectDesc.newBuilder();
        CollisionObjectDesc newMessage = builder.setCollisionShape("test").setFriction(1.0f).setGroup(1).addMask(1).setMass(2.0f).setRestitution(1.0f).setType(CollisionObjectType.COLLISION_OBJECT_TYPE_DYNAMIC).build();
        testReload(path, resource, newMessage, new IMessageHelper<CollisionResource, CollisionObjectDesc>() {
            public CollisionObjectDesc getMessage(CollisionResource resource) {
                return resource.getCollisionDesc();
            }
            public void print(CollisionObjectDesc message, StringWriter writer) throws IOException {
                TextFormat.print(message, writer);
            }
        });
    }

    @Test
    public void testConvexShape() throws CoreException, CreateException, IOException {
        String path = "logic/session/paddle.convexshape";
        ConvexShapeResource resource = (ConvexShapeResource)factory.load(monitor, path);
        ConvexShape.Builder builder = ConvexShape.newBuilder();
        ConvexShape newMessage = builder.setShapeType(ConvexShape.Type.TYPE_SPHERE).addData(1.0f).build();
        testReload(path, resource, newMessage, new IMessageHelper<ConvexShapeResource, ConvexShape>() {
            public ConvexShape getMessage(ConvexShapeResource resource) {
                return resource.getConvexShape();
            }
            public void print(ConvexShape message, StringWriter writer) throws IOException {
                TextFormat.print(message, writer);
            }
        });
    }

    @Test
    public void testLight() throws CoreException, CreateException, IOException {
        String path = "logic/test.light";
        LightResource resource = (LightResource)factory.load(monitor, path);
        LightDesc.Builder builder = LightDesc.newBuilder();
        Vector3 color = Vector3.newBuilder().setX(1.0f).setY(1.0f).setZ(1.0f).build();
        LightDesc newMessage = builder.setId("id").setColor(color).setIntensity(1.0f).setType(LightType.SPOT).setRange(1.0f).setDecay(100.0f).build();
        testReload(path, resource, newMessage, new IMessageHelper<LightResource, LightDesc>() {
            public LightDesc getMessage(LightResource resource) {
                return resource.getLightDesc();
            }
            public void print(LightDesc message, StringWriter writer) throws IOException {
                TextFormat.print(message, writer);
            }
        });
    }

    @Test
    public void testSprite() throws CoreException, CreateException, IOException {
        String path = "logic/session/ball.sprite";
        SpriteResource resource = (SpriteResource)factory.load(monitor, path);
        SpriteDesc.Builder builder = SpriteDesc.newBuilder();
        SpriteDesc newMessage = builder.setFrameCount(1).setFramesPerRow(1).setHeight(1.0f).setTexture("texture").setTileHeight(1).setTileWidth(1).setWidth(1.0f).build();
        testReload(path, resource, newMessage, new IMessageHelper<SpriteResource, SpriteDesc>() {
            public SpriteDesc getMessage(SpriteResource resource) {
                return resource.getSpriteDesc();
            }
            public void print(SpriteDesc message, StringWriter writer) throws IOException {
                TextFormat.print(message, writer);
            }
        });
    }

    private boolean equals(BufferedImage lhs, BufferedImage rhs) {
        if (lhs.getType() != rhs.getType()) {
            return false;
        }
        if (lhs.getWidth() != rhs.getWidth() || lhs.getHeight() != rhs.getHeight()) {
            return false;
        }
        for (int i = 0; i < lhs.getWidth(); ++i) {
            for (int j = 0; j < lhs.getHeight(); ++j) {
                if (lhs.getRGB(i, j) != rhs.getRGB(i, j)) {
                    return false;
                }
            }
        }
        return true;
    }

    @Test
    public void testTexture() throws CoreException, CreateException, IOException {
        String path = "graphics/ball.png";
        TextureResource resource = (TextureResource)factory.load(monitor, path);
        BufferedImage newImage = new BufferedImage(1, 1, resource.getImage().getType());
        newImage.setRGB(0, 0, 1);
        assertNotNull(resource);
        BufferedImage oldImage = resource.getImage();
        assertFalse(equals(newImage, oldImage));
        IFile imageIFile = project.getFile(path);
        ByteArrayOutputStream memOut = new ByteArrayOutputStream();
        MemoryCacheImageOutputStream imageOut = new MemoryCacheImageOutputStream(memOut);
        ImageIO.write(newImage, imageIFile.getFileExtension(), imageOut);
        ByteArrayInputStream memIn = new ByteArrayInputStream(memOut.toByteArray());
        imageIFile.setContents(memIn, 0, monitor);
        boolean equals = true;
        try {
            assertTrue(equals(newImage, resource.getImage()));
        } catch (AssertionError e) {
            equals = false;
        } finally {
            memOut = new ByteArrayOutputStream();
            ImageIO.write(oldImage, imageIFile.getFileExtension(), memOut);
            memIn = new ByteArrayInputStream(memOut.toByteArray());
            imageIFile.setContents(memIn, 0, monitor);
        }
        imageOut.close();
        assertTrue(equals);
        assertFalse(equals(newImage, resource.getImage()));
        assertTrue(equals(oldImage, resource.getImage()));
    }

}

