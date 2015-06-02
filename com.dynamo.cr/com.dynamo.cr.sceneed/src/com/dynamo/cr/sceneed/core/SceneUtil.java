package com.dynamo.cr.sceneed.core;

import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;

import javax.imageio.ImageIO;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.ui.preferences.PreferenceConstants;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class SceneUtil {
    public static void saveMessage(Message message, OutputStream contents, IProgressMonitor monitor, boolean newLineFormatOutput) throws IOException, CoreException {
        OutputStreamWriter writer = new OutputStreamWriter(contents);
        try {
            if (newLineFormatOutput) {
                // insert newline in output text file on textual newline (\n) occurrences
                // (google protocol buffers accepts double-quote + LR + double-quote newline scopes)
                writer.append(TextFormat.printToString(message).replace("\\n", "\\n\"\n  \""));
            } else {
                TextFormat.print(message, writer);
            }
            writer.flush();
        } finally {
            writer.close();
        }
    }

    public enum MouseType {
        ONE_BUTTON,
        THREE_BUTTON
    }

    public static MouseType getMouseType() {
        Activator activator = Activator.getDefault();
        if (activator != null) {
            IPreferenceStore store = Activator.getDefault().getPreferenceStore();
            String typeValue = store.getString(PreferenceConstants.P_MOUSE_TYPE);
            return MouseType.valueOf(typeValue);
        } else if (EditorUtil.isMac()) {
            return MouseType.ONE_BUTTON;
        } else {
            return MouseType.THREE_BUTTON;
        }
    }

    public static BufferedImage loadImage(IFile file) throws Exception {
        BufferedImage image = null;
        if (file != null && file.exists()) {
            InputStream is = file.getContents();
            try {
                BufferedImage origImage = ImageIO.read(is);
                image = new BufferedImage(origImage.getWidth(),
                        origImage.getHeight(), BufferedImage.TYPE_4BYTE_ABGR);
                Graphics2D g2d = image.createGraphics();
                g2d.drawImage(origImage, 0, 0, null);
                g2d.dispose();
            } finally {
                is.close();
            }
        }
        return image;
    }

    public static boolean showContextMenu(MouseEvent event) {
        switch (getMouseType()) {
        case ONE_BUTTON:
            // TODO One button currently fallback to three button: https://defold.fogbugz.com/default.asp?1718
            // Reason is that the line below conflicts with the camera movement (dolly on one-button)
            // return event.button == 1 && (event.stateMask & SWT.CTRL) != 0;
        case THREE_BUTTON:
            return event.button == 3 && (event.stateMask & SWT.MODIFIER_MASK) == 0;
        }
        return false;
    }
}
