package com.dynamo.cr.ddfeditor.test;

import org.junit.Test;

import com.dynamo.cr.ddfeditor.CameraEditor;
import com.dynamo.cr.ddfeditor.CollisionObjectEditor;
import com.dynamo.cr.ddfeditor.FontEditor;
import com.dynamo.cr.ddfeditor.GameObjectEditor;
import com.dynamo.cr.ddfeditor.GuiEditor;
import com.dynamo.cr.ddfeditor.InputBindingEditor;
import com.dynamo.cr.ddfeditor.LightEditor;
import com.dynamo.cr.ddfeditor.MaterialEditor;
import com.dynamo.cr.ddfeditor.ModelEditor;
import com.dynamo.cr.ddfeditor.RenderEditor;
import com.dynamo.cr.ddfeditor.SpawnPointEditor;
import com.dynamo.cr.ddfeditor.wizards.CollectionNewWizard;

public class NewDdfContentTest {

    @Test
    public void test() {
        // We create messages here in order to check that we aren't missing any required fields
        ModelEditor.newInitialWizardContent();
        GameObjectEditor.newInitialWizardContent();
        CameraEditor.newInitialWizardContent();
        LightEditor.newInitialWizardContent();
        SpawnPointEditor.newInitialWizardContent();
        CollisionObjectEditor.newInitialWizardContent();
        GuiEditor.newInitialWizardContent();
        FontEditor.newInitialWizardContent();
        MaterialEditor.newInitialWizardContent();
        RenderEditor.newInitialWizardContent();
        InputBindingEditor.newInitialWizardContent();
        CollectionNewWizard.newInitialWizardContent();
    }
}
