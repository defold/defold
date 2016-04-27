package com.defold.editor.eclipse;

import java.util.List;

import org.eclipse.fx.core.Util;
import org.eclipse.fx.ui.controls.styledtext.ActionEvent;
import org.eclipse.fx.ui.controls.styledtext.ActionEvent.ActionType;
import org.eclipse.fx.ui.controls.styledtext.StyledTextArea;
import org.eclipse.fx.ui.controls.styledtext.StyledTextLayoutContainer;
import org.eclipse.fx.ui.controls.styledtext.TextSelection;
import org.eclipse.fx.ui.controls.styledtext.VerifyEvent;
import org.eclipse.fx.ui.controls.styledtext.skin.StyledTextSkin.LineCell;
import org.eclipse.fx.ui.controls.styledtext.behavior.StyledTextBehavior;

import javafx.event.Event;
import javafx.geometry.Bounds;
import javafx.scene.input.KeyCode;
import javafx.scene.input.KeyEvent;
import javafx.scene.input.MouseEvent;

/**
 * Behavior for styled text
  Overrides for key event handlers until we can move to version 2.3.0
 */
public class DefoldStyledTextBehavior extends StyledTextBehavior{

	/**
	 * Create a new behavior
	 *
	 * @param styledText
	 *            the styled text control
	 */
	public DefoldStyledTextBehavior(StyledTextArea styledText) {
                super(styledText);
	}

        private int colx = 0;

       /**
        * Fix for keeping track of the horizontal caret position when
        * changing lines - Defold
        **/

        public void setPreferredColOffset(int x){
          colx =x;
        }

        public int getPreferredColOffset(){
          return colx;
        }

        /**
        *  Call to remember the horizonatal caret position when
        *  changing lines
        **/
        public void rememberCaretCol(int offset){
          int currentRowIndex = getControl().getContent().getLineAtOffset(offset);
          int colIdx = offset - getControl().getContent().getOffsetAtLine(currentRowIndex);
          System.out.println("Carin"  + colIdx);
          setPreferredColOffset(colIdx);
        }

	/**
	 * Handle key event
	 *
	 * @param arg0
	 *            the event
	 */
	protected void callActionForEvent(KeyEvent arg0) {
		if (arg0.getEventType() == KeyEvent.KEY_PRESSED) {
			defold_keyPressed(arg0);
		} else if (arg0.getEventType() == KeyEvent.KEY_TYPED) {
			defold_keyTyped(arg0);
		}
	}

        /**
	 * handle the mouse pressed
	 *
	 * @param arg0
	 *            the mouse event
	 */
	public void mousePressed(MouseEvent arg0) {
		getControl().requestFocus();
	}

       	/**
	 * Send a mouse pressed
	 *
	 * @param event
	 *            the event
	 * @param visibleCells
	 *            the visible cells
	 * @param selection
	 *            are we in selection mode
         *  Override for Defold to handle down/up width
	 */
	@SuppressWarnings("deprecation")
        @Override
	public void updateCursor(MouseEvent event, List<LineCell> visibleCells, boolean selection) {
            super.updateCursor(event, visibleCells, selection);
            rememberCaretCol(getControl().getCaretOffset());
        }



	@SuppressWarnings("deprecation")
	private void defold_keyPressed(KeyEvent event) {
		VerifyEvent evt = new VerifyEvent(getControl(), getControl(), event);
		Event.fireEvent(getControl(), evt);

		// Bug in JavaFX who enables the menu when ALT is pressed
		if (Util.isMacOS()) {
			if (event.getCode() == KeyCode.ALT || event.isAltDown()) {
				event.consume();
			}
		}

		if (evt.isConsumed()) {
			event.consume();
			return;
		}

		int currentRowIndex = getControl().getContent().getLineAtOffset(getControl().getCaretOffset());

		final int offset = getControl().getCaretOffset();

		switch (event.getCode()) {
		case SHIFT:
		case ALT:
		case CONTROL:
			break;
		case LEFT: {

                    /**
			if (event.isAltDown()) {
				invokeAction(ActionType.WORD_PREVIOUS);
			} else {
				if (offset == 0) {
					event.consume();
					break;
				}
				int newOffset = offset - 1;
				@SuppressWarnings("unused")
				int currentLine = getControl().getContent().getLineAtOffset(offset);
				@SuppressWarnings("unused")
				int newLine = getControl().getContent().getLineAtOffset(newOffset);
				getControl().impl_setCaretOffset(newOffset, event.isShiftDown());
				event.consume();
			}
                    **/

                        rememberCaretCol(getControl().getCaretOffset());
                        event.consume();
                        System.out.println("left");
                        break;

		}
		case RIGHT: {
                    /**
			if (event.isAltDown()) {
				invokeAction(ActionType.WORD_NEXT);
			} else if (event.isMetaDown()) {
				int currentLine = getControl().getContent().getLineAtOffset(offset);
				int lineOffset = getControl().getContent().getOffsetAtLine(currentLine);
				String lineContent = getControl().getContent().getLine(currentLine);

				getControl().impl_setCaretOffset(lineOffset + lineContent.length(), event.isShiftDown());
				event.consume();
			} else {
				if (offset + 1 > getControl().getContent().getCharCount()) {
					break;
				}
				int newOffset = offset + 1;
				// @SuppressWarnings("unused")
				// int currentLine =
				// getControl().getContent().getLineAtOffset(offset);
				// @SuppressWarnings("unused")
				// int newLine =
				// getControl().getContent().getLineAtOffset(newOffset);
				getControl().impl_setCaretOffset(newOffset, event.isShiftDown());
				event.consume();
			}
			break;
                    **/

                    rememberCaretCol(getControl().getCaretOffset());
                    event.consume();
                    System.out.println("right");
                    break;
		}
		case UP: {

			int rowIndex = currentRowIndex;

			if (rowIndex == 0) {
				break;
			}

			int colIdx = offset - getControl().getContent().getOffsetAtLine(rowIndex);
			rowIndex -= 1;

			int lineOffset = getControl().getContent().getOffsetAtLine(rowIndex);
			int newCaretPosition = lineOffset + colIdx;
			int maxPosition = lineOffset + getControl().getContent().getLine(rowIndex).length();

			getControl().impl_setCaretOffset(Math.min(newCaretPosition,
			maxPosition), event.isShiftDown());


                        System.out.println("up");
                        System.out.println("colx!"  + colx);
			event.consume();
			break;
		}
		case DOWN: {

                    /**
			int rowIndex = currentRowIndex;
			if (rowIndex + 1 == getControl().getContent().getLineCount()) {
				break;
			}

			int colIdx = offset - getControl().getContent().getOffsetAtLine(rowIndex);
			rowIndex += 1;

			int lineOffset = getControl().getContent().getOffsetAtLine(rowIndex);
			int newCaretPosition = lineOffset + colIdx;
			int maxPosition = lineOffset + getControl().getContent().getLine(rowIndex).length();

			getControl().impl_setCaretOffset(Math.min(newCaretPosition,
			maxPosition), event.isShiftDown());

                    System.out.println("down");
                    System.out.println("colx!"  + colx);
                    **/
			event.consume();
			break;
		}
		case ENTER:
			if (getControl().getEditable()) {
				int line = getControl().getContent().getLineAtOffset(getControl().getCaretOffset());
				String lineContent = getControl().getContent().getLine(line);

				// FIXME Temp hack
				char[] chars = lineContent.toCharArray();
				String prefix = ""; //$NON-NLS-1$
				for (int i = 0; i < chars.length; i++) {
					if (chars[i] == ' ') {
						prefix += " "; //$NON-NLS-1$
					} else {
						break;
					}
				}

				String newLine = System.getProperty("line.separator"); //$NON-NLS-1$

				getControl().getContent().replaceTextRange(getControl().getCaretOffset(), 0, newLine + prefix);
				// listView.getSelectionModel().select(listView.getSelectionModel().getSelectedIndex()+1);
				getControl().setCaretOffset(offset + newLine.length() + prefix.length());
			}
			break;
		case DELETE:
			if (getControl().getEditable()) {
				if (event.isMetaDown()) {
					invokeAction(ActionType.DELETE_WORD_NEXT);
				} else {
					getControl().getContent().replaceTextRange(getControl().getCaretOffset(), 1, ""); //$NON-NLS-1$
					getControl().setCaretOffset(offset);
				}
				break;
			}
		case BACK_SPACE:
			if (getControl().getEditable()) {
				if (event.isMetaDown()) {
					invokeAction(ActionType.DELETE_WORD_PREVIOUS);
				} else {
					TextSelection selection = getControl().getSelection();
					if (selection.length > 0) {
						getControl().getContent().replaceTextRange(selection.offset, selection.length, ""); //$NON-NLS-1$
						getControl().setCaretOffset(selection.offset);
					} else {
						getControl().getContent().replaceTextRange(getControl().getCaretOffset() - 1, 1, ""); //$NON-NLS-1$
						getControl().setCaretOffset(offset - 1);
					}
				}
				break;
			}
		case TAB:
			if (getControl().getEditable()) {
				event.consume();
				if (event.isShiftDown()) {
					// TODO Remove first 4 white space chars???
					break;
				} else {
					getControl().getContent().replaceTextRange(getControl().getCaretOffset(), 0, "\t"); //$NON-NLS-1$
					getControl().setCaretOffset(offset + 1);
					break;
				}
			}
		case V:
			if (getControl().getEditable()) {
				if (event.isShortcutDown()) {
                                    //getControl().paste();
                                    //we are handling this through the graph
					event.consume();
					break;
				}
			}
		case X:
			if (getControl().getEditable()) {
				if (event.isShortcutDown()) {
					getControl().cut();
					event.consume();
					break;
				}
			}
		case C: {
			if (event.isShortcutDown()) {
                            //getControl().copy();
                            //we are handling this through the graph
				event.consume();
				break;
			}
		}
		default:
			break;
		}
	}

        private void defold_keyTyped(KeyEvent event) {
		if (getControl().getEditable()) {

			String character = event.getCharacter();
			if (character.length() == 0) {
				return;
			}

			// check the modifiers
			// - OS-X: ALT+L ==> @
			// - win32/linux: ALTGR+Q ==> @
			if (event.isControlDown() || event.isAltDown() || (Util.isMacOS() && event.isMetaDown())) {
				if (!((event.isControlDown() || Util.isMacOS()) && event.isAltDown()))
					return;
			}

			if (character.charAt(0) > 31 // No ascii control chars
					&& character.charAt(0) != 127 // no delete key
					&& !event.isMetaDown()) {
				final int offset = getControl().getCaretOffset();
				getControl().getContent().replaceTextRange(getControl().getCaretOffset(), 0, character);
                                final int newOffset = offset + 1;
				getControl().setCaretOffset(newOffset);
                                rememberCaretCol(newOffset);
                        }

		}
	}

}
