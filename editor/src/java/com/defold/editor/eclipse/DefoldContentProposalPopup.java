package com.defold.editor.eclipse;

import java.util.Collections;
import java.util.List;
import java.util.function.Function;

import org.eclipse.fx.core.Util;
import org.eclipse.fx.text.ui.ITextViewer;
import org.eclipse.fx.ui.controls.list.SimpleListCell;
import org.eclipse.jface.text.BadLocationException;
import org.eclipse.jface.text.IDocument;

import javafx.application.Platform;
import javafx.collections.FXCollections;
import javafx.geometry.Point2D;
import javafx.scene.Node;
import javafx.scene.control.ListView;
import javafx.scene.input.KeyCode;
import javafx.scene.input.KeyEvent;
import javafx.scene.layout.BorderPane;
import javafx.stage.PopupWindow;

import org.eclipse.fx.text.ui.contentassist.*;

public class DefoldContentProposalPopup {
	private ITextViewer viewer;
	private PopupWindow stage;
	private ListView<ICompletionProposal> proposalList;
	private String prefix;
	private int offset;
	private Function<ContentAssistContextData, List<ICompletionProposal>> proposalComputer;

	public DefoldContentProposalPopup(ITextViewer viewer, Function<ContentAssistContextData, List<ICompletionProposal>> proposalComputer) {
		this.viewer = viewer;
		this.proposalComputer = proposalComputer;
	}

	public void displayProposals(List<ICompletionProposal> proposalList, int offset, Point2D position) {
		setup();
		this.prefix = ""; //$NON-NLS-1$
		this.offset = offset;
		this.proposalList.setItems(FXCollections.observableArrayList(proposalList));
		this.proposalList.getSelectionModel().select(0);
		this.stage.setX(position.getX());
		this.stage.setY(position.getY());
		this.stage.setWidth(300);
		this.stage.setHeight(200);
		this.stage.show(this.viewer.getTextWidget().getScene().getWindow());
		this.stage.requestFocus();
	}

	private void handleKeyTyped(KeyEvent event) {
		if( event.getCharacter().length() == 0 ) {
			return;
		}

		String character = event.getCharacter();
		if( character.length() == 0 ) {
			return;
		}

		if (event.isControlDown() || event.isAltDown() || (Util.isMacOS() && event.isMetaDown())) {
			if (!((event.isControlDown() || Util.isMacOS()) && event.isAltDown())) {
				return;
			}
		}

		if (character.charAt(0) > 0x1F
	            && character.charAt(0) != 0x7F ) {
//			try {
				this.prefix = this.prefix+character;
//				viewer.getDocument().replace(offset, 0, character);
				this.offset += event.getCharacter().length();
//				viewer.getTextWidget().setCaretOffset(offset);
				updateProposals();
//			} catch (BadLocationException e) {
//				// TODO Auto-generated catch block
//				e.printStackTrace();
//			}
		}
	}

	private void updateProposals() {
		List<ICompletionProposal> list = this.proposalComputer.apply(new ContentAssistContextData(this.offset,this.viewer.getDocument()/*,prefix*/));
		if( ! list.isEmpty() ) {
			this.proposalList.setItems(FXCollections.observableArrayList(list));
			this.proposalList.scrollTo(0);
			this.proposalList.getSelectionModel().select(0);
		} else {
			this.stage.hide();
		}
	}

	private void handleKeyPressed(KeyEvent event) {
		if( event.getCode() == KeyCode.ESCAPE ) {
			event.consume();
			this.stage.hide();
		} else if( event.getCode() == KeyCode.BACK_SPACE ) {
			event.consume();
			this.offset -= 1;
			try {
				this.viewer.getDocument().replace(this.offset, 1, ""); //$NON-NLS-1$
				this.viewer.getTextWidget().setCaretOffset(this.offset);
				updateProposals();
			} catch (BadLocationException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		} else if( event.getCode() == KeyCode.ENTER ) {
			event.consume();
			applySelection();
		} else if( event.getCode() == KeyCode.LEFT ) {
			event.consume();
			this.offset -= 1;
			this.offset = Math.max(0, this.offset);
			this.viewer.getTextWidget().setCaretOffset(this.offset);
			updateProposals();
		} else if( event.getCode() == KeyCode.RIGHT ) {
			event.consume();
			this.offset += 1;
			this.offset = Math.min(this.viewer.getDocument().getLength()-1, this.offset);
			this.viewer.getTextWidget().setCaretOffset(this.offset);
			updateProposals();
		}
	}

	private void applySelection() {
		ICompletionProposal selectedItem = this.proposalList.getSelectionModel().getSelectedItem();
		if( selectedItem != null ) {
			IDocument document = this.viewer.getDocument();
			selectedItem.apply(document);
			this.viewer.getTextWidget().setSelection(selectedItem.getSelection(document));
			this.stage.hide();
		}
	}

	private void setup() {
		if( this.stage == null ) {
			this.stage = new PopupWindow() {
				// empty by design
			};
			this.stage.setAutoFix(false);
			this.stage.setWidth(300);
			this.stage.setHeight(200);
			BorderPane p = new BorderPane();
			p.setPrefHeight(200);
			p.setPrefWidth(400);
			this.stage.getScene().addEventFilter(KeyEvent.KEY_TYPED, this::handleKeyTyped);
			this.stage.getScene().addEventFilter(KeyEvent.KEY_PRESSED, this::handleKeyPressed);

                        System.err.println("Carin adding" + this.viewer.getTextWidget().getScene().getRoot().getStylesheets());
			this.proposalList = new ListView<>();
			//this.stage.getScene().getStylesheets().addAll(this.viewer.getTextWidget().getScene().getStylesheets());
			this.proposalList.setId("proposals"); //$NON-NLS-1$
                        //this.proposalList.setStyle("-fx-background-color: #3b3e43; -fx-padding: 10;");
			this.proposalList.setOnMouseClicked((e) -> {
				if(e.getClickCount() == 1) {
					applySelection();
				}
			});

                  	this.stage.getScene().getStylesheets().addAll(this.viewer.getTextWidget().getScene().getRoot().getStylesheets());


			Function<ICompletionProposal, CharSequence> label = (c) -> c.getLabel();
			Function<ICompletionProposal, Node> graphic = (c) -> c.getGraphic();
			Function<ICompletionProposal, List<String>> css = (c) -> Collections.emptyList();


			this.proposalList.setCellFactory((v) -> new SimpleListCell<ICompletionProposal>(label,graphic,css));
			p.setCenter(this.proposalList);
			this.stage.getScene().setRoot(p);
			this.stage.focusedProperty().addListener((o) -> {
				if( this.stage != null && ! this.stage.isFocused() ) {
					Platform.runLater(this.stage::hide);
				}
			});
			// Fix CSS warnings
			this.stage.setOnHidden((o) -> {
				this.stage = null;
			});
		}
	}
}
