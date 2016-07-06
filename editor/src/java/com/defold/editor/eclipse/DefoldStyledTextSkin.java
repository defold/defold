package com.defold.editor.eclipse;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.eclipse.fx.ui.controls.styledtext.StyleRange;
import org.eclipse.fx.ui.controls.styledtext.StyledTextArea;
import org.eclipse.fx.ui.controls.styledtext.StyledTextArea.StyledTextLine;
import org.eclipse.fx.ui.controls.styledtext.TextSelection;
import org.eclipse.jdt.annotation.NonNull;

import com.sun.javafx.scene.control.skin.ListViewSkin;
import com.sun.javafx.scene.control.skin.VirtualFlow;

import javafx.application.Platform;
import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.collections.FXCollections;
import javafx.collections.ObservableList;
import javafx.event.EventHandler;
import javafx.geometry.Point2D;
import javafx.geometry.Pos;
import javafx.scene.Node;
import javafx.scene.control.Label;
import javafx.scene.control.ListCell;
import javafx.scene.control.ListView;
import javafx.scene.control.Skin;
import javafx.scene.control.SkinBase;
import javafx.scene.input.MouseEvent;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Pane;
import javafx.scene.layout.Priority;
import javafx.scene.layout.Region;
import javafx.util.Callback;

/**
 * Styled text skin
 */
@SuppressWarnings("restriction")
public class DefoldStyledTextSkin extends SkinBase<StyledTextArea> {
	ListView<Line> contentView;
	LineRuler lineRuler;

	ObservableList<Line> lineList = FXCollections.observableArrayList();

	// private Set<LineCell> visibleCells = new HashSet<>();
	Map<LineCell, LineInfo> lineInfoMap = new HashMap<>();

	HBox rootContainer;

	private final DefoldStyledTextBehavior behavior;

	/**
	 * Create a new skin
	 *
	 * @param defoldStyledTextArea
	 *            the control
	 */
	public DefoldStyledTextSkin(StyledTextArea defoldStyledTextArea) {
		this(defoldStyledTextArea, new DefoldStyledTextBehavior(defoldStyledTextArea));
	}

	/**
	 * Create a new skin
	 *
	 * @param styledText
	 *            the styled text
	 * @param behavior
	 *            the behavior
	 */
	public DefoldStyledTextSkin(StyledTextArea styledText, DefoldStyledTextBehavior behavior) {
		super(styledText);
		this.behavior = behavior;
		this.rootContainer = new HBox();
		this.rootContainer.setSpacing(0);

		this.lineRuler = new LineRuler();
		this.lineRuler.visibleProperty().bind(getSkinnable().lineRulerVisibleProperty());
		this.lineRuler.managedProperty().bind(getSkinnable().lineRulerVisibleProperty());
		this.rootContainer.getChildren().add(this.lineRuler);

		this.contentView = new ListView<Line>() {
			@Override
			protected Skin<?> createDefaultSkin() {
				return new MyListViewSkin(this);
			}
		};
		styledText.addEventHandler(MouseEvent.MOUSE_PRESSED, behavior::mousePressed);

		initializeContentViewer(this.contentView);

		recalculateItems();

		this.contentView.setItems(this.lineList);

		HBox.setHgrow(this.contentView, Priority.ALWAYS);

		this.rootContainer.getChildren().addAll(this.contentView);
		getChildren().addAll(this.rootContainer);

		styledText.caretOffsetProperty().addListener(new ChangeListener<Number>() {

			@Override
			public void changed(ObservableValue<? extends Number> observable, Number oldValue, Number newValue) {
				int lineIndex = getSkinnable().getContent().getLineAtOffset(newValue.intValue());
                                if (lineIndex < 0){
                                    return;
                                 }
				Line lineObject = DefoldStyledTextSkin.this.lineList.get(lineIndex);
                                if (lineObject == null){
                                    return;
                                }

				getFlow().show(lineIndex);

				for (LineCell c : lineInfoMap.keySet()) {
					if (c.domainElement == lineObject) {
						// Adjust the selection
						if (DefoldStyledTextSkin.this.contentView.getSelectionModel().getSelectedItem() != c.domainElement) {
							DefoldStyledTextSkin.this.contentView.getSelectionModel().select(lineObject);
						}

						DefoldStyledTextLayoutContainer p = (DefoldStyledTextLayoutContainer) c.getGraphic();
						p.setCaretIndex(newValue.intValue() - p.getStartOffset());
						p.requestLayout();

						updateCurrentCursorNode(p);

						return;
					}
				}

			}
		});
		styledText.selectionProperty().addListener(new ChangeListener<TextSelection>() {

			@Override
			public void changed(ObservableValue<? extends TextSelection> observable, TextSelection oldValue, TextSelection newValue) {
				Map<LineCell,LineInfo > mymap = lineInfoMap;
				Set<LineCell> mymapcells = mymap.keySet();

				if (newValue == null || newValue.length == 0) {
					for (LineCell c : mymapcells) {
						if (c.getGraphic() != null) {
							DefoldStyledTextLayoutContainer block = (DefoldStyledTextLayoutContainer) c.getGraphic();
							block.setSelection(new TextSelection(0, 0));
						}
					}
				} else {
					TextSelection selection = newValue;
					for (LineCell c : mymapcells) {
						if (c.getGraphic() != null) {
							Line arg0 = c.domainElement;
							DefoldStyledTextLayoutContainer block = (DefoldStyledTextLayoutContainer) c.getGraphic();
							if (selection.length > 0 && block.intersectOffset(selection.offset, selection.offset + selection.length)) {
								int start = Math.max(0, selection.offset - arg0.getLineOffset());

								if (arg0.getLineOffset() + arg0.getLineLength() > selection.offset + selection.length) {
									block.setSelection(new TextSelection(start, selection.offset + selection.length - arg0.getLineOffset() - start));
								} else {
									block.setSelection(new TextSelection(start, arg0.getLineLength() - start));
								}
							} else {
								block.setSelection(new TextSelection(0, 0));
							}
						}
					}
				}
			}
		});
	}

	public ListView<Line> getListView(){
		return contentView;
	}
	
	DefoldStyledTextBehavior getBehavior() {
		return this.behavior;
	}

	/**
	 * Set up the content viewer
	 *
	 * @param contentView
	 *            the content viewer
	 */
	protected void initializeContentViewer(ListView<Line> contentView) {
		// this.contentView.getStyleClass().add("styled-text-area");
		// //$NON-NLS-1$
		// listView.setFocusTraversable(false);
		contentView.focusedProperty().addListener(new ChangeListener<Boolean>() {

			@Override
			public void changed(ObservableValue<? extends Boolean> observable, Boolean oldValue, Boolean newValue) {
				if (newValue.booleanValue()) {
					getSkinnable().requestFocus();
				}
			}
		});
		// listView.addEventHandler(KeyEvent.KEY_PRESSED, new
		// EventHandler<KeyEvent>() {
		// });
		contentView.setCellFactory(new Callback<ListView<Line>, ListCell<Line>>() {

			@Override
			public ListCell<Line> call(ListView<Line> arg0) {
				return new LineCell();
			}
		});
		contentView.setMinHeight(0);
		contentView.setMinWidth(0);
		// this.contentView.setFixedCellSize(value);
		// this.contentView.setFixedCellSize(15);
		contentView.setOnMousePressed(new EventHandler<MouseEvent>() {

			@Override
			public void handle(MouseEvent event) {
				getBehavior().defoldUpdateCursor(event, getCurrentVisibleCells(), event.isShiftDown());
				// The consuming does not help because it looks like the
				// selection change happens earlier => should be push a new
				// ListViewBehavior?
				event.consume();
			}
		});
		contentView.setOnMouseDragged(new EventHandler<MouseEvent>() {

			@Override
			public void handle(MouseEvent event) {
				getBehavior().defoldUpdateCursor(event, getCurrentVisibleCells(), true);
				event.consume();
			}
		});
	}

	private DefoldStyledTextLayoutContainer currentActiveNode;

	void updateCurrentCursorNode(DefoldStyledTextLayoutContainer node) {
		if (this.currentActiveNode != node) {
			if (this.currentActiveNode != null) {
				this.currentActiveNode.setCaretIndex(-1);
			}

			this.currentActiveNode = node;
		}
	}

	/**
	 * Refresh the line ruler
	 */
	public void refreshLineRuler() {
		this.lineRuler.refresh();
	}

	MyVirtualFlow getFlow() {
		if (this.contentView == null || this.contentView.getSkin() == null) {
			return null;
		}
		return ((MyListViewSkin) this.contentView.getSkin()).getFlow();
	}

	/**
	 * The line height at the care position
	 *
	 * @param caretPosition
	 *            the position
	 * @return the line height
	 */
	public double getLineHeight(int caretPosition) {
		int lineIndex = getSkinnable().getContent().getLineAtOffset(caretPosition);
		Line lineObject = this.lineList.get(lineIndex);

		for (LineCell c : getCurrentVisibleCells()) {
			if (c.domainElement == lineObject) {
				return c.getHeight();
			}
		}
		return 0;
	}

	/**
	 * Get the point for the caret position
	 *
	 * @param caretPosition
	 *            the position
	 * @return the point
	 */
	public Point2D getCaretLocation(int caretPosition) {
		if (caretPosition < 0) {
			return null;
		}

		int lineIndex = getSkinnable().getContent().getLineAtOffset(caretPosition);
		Line lineObject = this.lineList.get(lineIndex);
		for (LineCell c : getCurrentVisibleCells()) {
			if (c.domainElement == lineObject) {
				DefoldStyledTextLayoutContainer b = (DefoldStyledTextLayoutContainer) c.getGraphic();
				Point2D careLocation = b.getCareLocation(caretPosition - b.getStartOffset());
				Point2D tmp = getSkinnable().sceneToLocal(b.localToScene(careLocation));
				return new Point2D(tmp.getX(), getSkinnable().sceneToLocal(b.localToScene(0, b.getHeight())).getY());
			}
		}

		return null;
	}

	/**
	 * Compute the min height
	 *
	 * @param width
	 *            the width that should be used if minimum height depends on it
	 * @return the min height
	 */
	protected double computeMinHeight(double width) {
		return this.contentView.minHeight(width);
	}

	/**
	 * Compute the min width
	 *
	 * @param height
	 *            the height that should be used if minimum width depends on it
	 * @return the min width
	 */
	protected double computeMinWidth(double height) {
		return this.contentView.minWidth(height);
	}

	/**
	 * recalculate the line items
	 */
	public void recalculateItems() {
		if (this.lineList.size() != getSkinnable().getContent().getLineCount()) {
			if (this.lineList.size() > getSkinnable().getContent().getLineCount()) {
				this.lineList.remove(getSkinnable().getContent().getLineCount(), this.lineList.size());
			} else {
				List<Line> tmp = new ArrayList<>(getSkinnable().getContent().getLineCount() - this.lineList.size());
				for (int i = this.lineList.size(); i < getSkinnable().getContent().getLineCount(); i++) {
					tmp.add(new Line());
				}
				// System.err.println("DOING AN ADD!!!!!");
				this.lineList.addAll(tmp);
				// System.err.println("ADD IS DONE!");
			}
		}

		redraw();
	}

	/**
	 * Redraw the lines
	 */
	public void redraw() {
		for (LineCell l : getCurrentVisibleCells()) {
			if (l != null)
				l.update();
		}
	}

	List<LineCell> getCurrentVisibleCells() {
		if (this.contentView == null || this.contentView.getSkin() == null) {
			return Collections.emptyList();
		}
		return ((MyListViewSkin) this.contentView.getSkin()).getFlow().getCells();
	}

	/**
	 * A line cell
	 */
	public class LineCell extends ListCell<Line> {
		Line domainElement;
		List<Segment> currentSegments;

		/**
		 * A line cell instance
		 */
		public LineCell() {
			getStyleClass().add("styled-text-line"); //$NON-NLS-1$
		}

		/**
		 * @return the domain element
		 */
		public Line getDomainElement() {
			return this.domainElement;
		}

		/**
		 * Update the item
		 */
		public void update() {
			if (this.domainElement != null) {
				updateItem(this.domainElement, false);
			}
		}

		/**
		 * Update the caret
		 */
		public void updateCaret() {
			int caretPosition = getSkinnable().getCaretOffset();

			if (caretPosition < 0) {
				return;
			}
		}

		@Override
		protected void updateItem(Line arg0, boolean arg1) {
			if (arg0 != null && !arg1) {
				this.domainElement = arg0;
				LineInfo lineInfo = DefoldStyledTextSkin.this.lineInfoMap.get(this);
				if (lineInfo == null) {
					lineInfo = new LineInfo();
					lineInfo.setDomainElement(this.domainElement);
					DefoldStyledTextSkin.this.lineInfoMap.put(this, lineInfo);
					DefoldStyledTextSkin.this.lineRuler.getChildren().add(lineInfo);
					DefoldStyledTextSkin.this.lineRuler.requestLayout();
				} else {
					lineInfo.setDomainElement(this.domainElement);
					DefoldStyledTextSkin.this.lineRuler.requestLayout();
				}
				lineInfo.setLayoutY(getLayoutY());

				DefoldStyledTextLayoutContainer block = (DefoldStyledTextLayoutContainer) getGraphic();

				if (block == null) {
					// System.err.println("CREATING NEW GRAPHIC BLOCK: " + this
					// + " => " + this.domainElement);
					block = new DefoldStyledTextLayoutContainer(getSkinnable().focusedProperty());
					block.getStyleClass().add("source-segment-container"); //$NON-NLS-1$
					setGraphic(block);
					// getSkinnable().requestLayout();
				}
				block.setStartOffset(arg0.getLineOffset());

				List<Segment> segments = arg0.getSegments();
				if (segments.equals(this.currentSegments)) {
					// System.err.println("EQUAL: " + this.currentSegments + "
					// vs " + segments); //$NON-NLS-1$
					return;
				} else {
					// System.err.println("MODIFIED: " + this.currentSegments +
					// " vs " + segments);
				}

				this.currentSegments = segments;

				List<@NonNull DefoldStyledTextNode> texts = new ArrayList<>();

				for (final Segment seg : this.currentSegments) {
					DefoldStyledTextNode t = new DefoldStyledTextNode(seg.text);
					if (seg.style.stylename != null) {
						if (seg.style.stylename.contains(".")) { //$NON-NLS-1$
							List<String> styles = new ArrayList<String>(Arrays.asList(seg.style.stylename.split("\\."))); //$NON-NLS-1$
							styles.add(0, "source-segment"); //$NON-NLS-1$
							t.getStyleClass().setAll(styles);
						} else {
							t.getStyleClass().setAll("source-segment", seg.style.stylename); //$NON-NLS-1$
						}

					} else {
						if (seg.style.foreground != null) {
							t.getStyleClass().setAll("plain-source-segment"); //$NON-NLS-1$
						} else {
							t.getStyleClass().setAll("source-segment"); //$NON-NLS-1$
						}
					}
					texts.add(t);
				}

				if (segments.isEmpty()) {
					DefoldStyledTextNode t = new DefoldStyledTextNode(""); //$NON-NLS-1$
					t.getStyleClass().setAll("source-segment"); //$NON-NLS-1$
					block.getTextNodes().setAll(t);
				} else {
					block.getTextNodes().setAll(texts);
				}

				TextSelection selection = getSkinnable().getSelection();

				if (selection.length > 0 && block.intersectOffset(selection.offset, selection.offset + selection.length)) {
					int start = Math.max(0, selection.offset - arg0.getLineOffset());

					if (arg0.getLineOffset() + arg0.getLineLength() > selection.offset + selection.length) {
						block.setSelection(new TextSelection(start, selection.offset + selection.length - arg0.getLineOffset() - start));
					} else {
						block.setSelection(new TextSelection(start, arg0.getLineLength() - start));
					}
				} else {
					block.setSelection(new TextSelection(0, 0));
				}

				if (arg0.getLineOffset() <= getSkinnable().getCaretOffset() && arg0.getLineOffset() + arg0.getText().length() >= getSkinnable().getCaretOffset()) {
					block.setCaretIndex(getSkinnable().getCaretOffset() - arg0.getLineOffset());
					updateCurrentCursorNode(block);
				} else {
					block.setCaretIndex(-1);
				}
			} else {
				// FIND OUT WHY WE CLEAR SO OFTEN
				// System.err.println("CLEARING GRAPHICS: " + this + " => " +
				// this.domainElement);

				setGraphic(null);
				this.domainElement = null;
				this.currentSegments = null;
				LineInfo lineInfo = DefoldStyledTextSkin.this.lineInfoMap.remove(this);
				if (lineInfo != null) {
					lineInfo.setDomainElement(null);
				}
			}

			super.updateItem(arg0, arg1);
		}
	}

	static class RegionImpl extends Region {
		public RegionImpl(Node... nodes) {
			getChildren().addAll(nodes);
		}

		@Override
		public ObservableList<Node> getChildren() {
			return super.getChildren();
		}
	}

	/**
	 * The line domain object
	 */
	public class Line implements StyledTextLine {
		/**
		 * @return the current text
		 */
		@Override
		public String getText() {
			return removeLineending(getSkinnable().getContent().getLine(DefoldStyledTextSkin.this.lineList.indexOf(this)));
		}

		@Override
		public int getLineIndex() {
			return DefoldStyledTextSkin.this.lineList.indexOf(this);
		}

		/**
		 * @return the line offset
		 */
		public int getLineOffset() {
			int idx = DefoldStyledTextSkin.this.lineList.indexOf(this);
			return getSkinnable().getContent().getOffsetAtLine(idx);
		}

		/**
		 * @return the line length
		 */
		public int getLineLength() {
			int idx = DefoldStyledTextSkin.this.lineList.indexOf(this);
			String s = getSkinnable().getContent().getLine(idx);
			return s.length();
		}

		/**
		 * @return the different segments
		 */
		@SuppressWarnings("null")
		public List<Segment> getSegments() {
			int idx = DefoldStyledTextSkin.this.lineList.indexOf(this);
			List<Segment> segments = new ArrayList<>();

			String line = getSkinnable().getContent().getLine(idx);
			if (line != null) {
				int start = getSkinnable().getContent().getOffsetAtLine(idx);
				int length = line.length();

				StyleRange[] ranges = getSkinnable().getStyleRanges(start, length, true);
				if (ranges == null) {
					return Collections.emptyList();
				}

				if (ranges.length == 0 && line.length() > 0) {
					StyleRange styleRange = new StyleRange((String) null);
					styleRange.start = start;
					styleRange.length = line.length();

					Segment seg = new Segment();
					seg.text = removeLineending(line.substring(0, line.length()));
					seg.style = styleRange;

					segments.add(seg);
				} else {
					int lastIndex = -1;

					if (ranges.length > 0) {
						if (ranges[0].start - start > 0) {
							StyleRange styleRange = new StyleRange((String) null);
							styleRange.start = start;
							styleRange.length = ranges[0].start - start;

							Segment seg = new Segment();
							seg.text = removeLineending(line.substring(0, ranges[0].start - start));
							seg.style = styleRange;

							segments.add(seg);
						}
					}

					for (StyleRange r : ranges) {
						int begin = r.start - start;
						int end = r.start - start + r.length;

						if (lastIndex != -1 && lastIndex != begin) {
							StyleRange styleRange = new StyleRange((String) null);
							styleRange.start = start + lastIndex;
							styleRange.length = begin - lastIndex;

							Segment seg = new Segment();
							seg.text = removeLineending(line.substring(lastIndex, begin));
							seg.style = styleRange;

							segments.add(seg);
						}

						Segment seg = new Segment();
						seg.text = removeLineending(line.substring(begin, end));
						seg.style = r;

						segments.add(seg);
						lastIndex = end;
					}

					if (lastIndex > 0 && lastIndex < line.length()) {
						StyleRange styleRange = new StyleRange((String) null);
						styleRange.start = start + lastIndex;
						styleRange.length = line.length() - lastIndex;

						Segment seg = new Segment();
						seg.text = removeLineending(line.substring(lastIndex, line.length()));
						seg.style = styleRange;

						segments.add(seg);
					}
				}
			}

			return segments;
		}
	}

	class Segment {
		public String text;
		public StyleRange style;

		@Override
		public String toString() {
			return this.text + " => " + this.style; //$NON-NLS-1$
		}

		@Override
		public int hashCode() {
			final int prime = 31;
			int result = 1;
			result = prime * result + getOuterType().hashCode();
			result = prime * result + ((this.style == null) ? 0 : this.style.hashCode());
			result = prime * result + ((this.text == null) ? 0 : this.text.hashCode());
			return result;
		}

		@Override
		public boolean equals(Object obj) {
			if (this == obj)
				return true;
			if (obj == null)
				return false;
			if (getClass() != obj.getClass())
				return false;
			Segment other = (Segment) obj;
			if (!getOuterType().equals(other.getOuterType()))
				return false;
			if (this.style == null) {
				if (other.style != null)
					return false;
			} else if (!this.style.equals(other.style))
				return false;
			if (this.text == null) {
				if (other.text != null)
					return false;
			} else if (!this.text.equals(other.text))
				return false;
			return true;
		}

		private DefoldStyledTextSkin getOuterType() {
			return DefoldStyledTextSkin.this;
		}
	}

	static String removeLineending(String s) {
		return s.replace("\n", "").replace("\r", ""); //$NON-NLS-1$//$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
	}

	class LineInfo extends HBox {
		private Label markerLabel;
		private Label lineText;
		Line line;

		public LineInfo() {
			this.markerLabel = new Label();
			this.markerLabel.setPrefWidth(20);
			this.lineText = new Label();
			this.lineText.getStyleClass().add("line-ruler-text"); //$NON-NLS-1$
			this.lineText.setMaxWidth(Double.MAX_VALUE);
			this.lineText.setMaxHeight(Double.MAX_VALUE);
			this.lineText.setAlignment(Pos.CENTER_RIGHT);
			HBox.setHgrow(this.lineText, Priority.ALWAYS);
			getChildren().addAll(this.markerLabel, this.lineText);
		}

		public void setDomainElement(Line line) {
			if (line == null) {
				this.line = null;
				setManaged(false);
				DefoldStyledTextSkin.this.rootContainer.requestLayout();
			} else {
				if (line != this.line) {
					this.line = line;
					calculateContent();
				}
			}
		}

		public void calculateContent() {
			Line line = this.line;
			if (line != null) {
				setManaged(true);
				String newText = this.line.getLineIndex() + 1 + ""; //$NON-NLS-1$
				String oldText = this.lineText.getText();
				if (oldText == null) {
					oldText = ""; //$NON-NLS-1$
				}
				this.lineText.setText(newText);
				this.markerLabel.setGraphic(getSkinnable().getLineRulerGraphicNodeFactory().call(line));
				if (newText.length() != oldText.length()) {
					DefoldStyledTextSkin.this.rootContainer.requestLayout();
				}
				DefoldStyledTextSkin.this.lineRuler.layout();
			}
		}
	}

	class LineRuler extends Pane {
		boolean skipRelayout;

		@Override
		protected void layoutChildren() {
			if (this.skipRelayout) {
				return;
			}
			super.layoutChildren();
			Set<Node> children = new HashSet<Node>(getChildren());
			List<LineInfo> layouted = new ArrayList<>();
			double maxWidth = 0;
			for (LineCell c : lineInfoMap.keySet()) {
				if (c.isVisible()) {
					LineInfo lineInfo = DefoldStyledTextSkin.this.lineInfoMap.get(c);
					if (lineInfo != null) {
						layouted.add(lineInfo);
						maxWidth = Math.max(maxWidth, lineInfo.getWidth());
						lineInfo.relocate(0, c.getLayoutY());
						lineInfo.resize(lineInfo.getWidth(), c.getHeight());
						lineInfo.setVisible(true);
						children.remove(lineInfo);
					}
				}
				if (c.getGraphic() != null) {
					DefoldStyledTextLayoutContainer block = (DefoldStyledTextLayoutContainer) c.getGraphic();
					block.requestLayout();
				}
			}

			for (LineInfo l : layouted) {
				l.resize(maxWidth, l.getHeight());

			}

			List<Node> toRemove = new ArrayList<>();
			for (Node n : children) {
				if (n instanceof LineInfo) {
					LineInfo l = (LineInfo) n;
					if (l.line == null) {
						toRemove.add(l);
					}
				}
				n.setVisible(false);
			}

			getChildren().removeAll(toRemove);
		}

		public void refresh() {
			new ArrayList<>(getChildren()).stream().filter(n -> n instanceof LineInfo).forEach(n -> ((LineInfo) n).calculateContent());
		}
	}

	class MyListViewSkin extends ListViewSkin<Line> {
		private MyVirtualFlow flow;

		public MyListViewSkin(ListView<Line> listView) {
			super(listView);
		}

		public MyVirtualFlow getFlow() {
			return this.flow;
		}

		@SuppressWarnings("unchecked")
		@Override
		protected VirtualFlow<ListCell<Line>> createVirtualFlow() {
			this.flow = new MyVirtualFlow();
			return (VirtualFlow<ListCell<Line>>) ((VirtualFlow<?>) this.flow);
		}

	}

	class MyVirtualFlow extends VirtualFlow<LineCell> {
		public MyVirtualFlow() {
		}

		@Override
		protected void layoutChildren() {
			super.layoutChildren();

			Platform.runLater(new Runnable() {

				@Override
				public void run() {
					DefoldStyledTextSkin.this.lineRuler.requestLayout();
				}
			});

		}


		@Override
		protected void positionCell(LineCell cell, double position) {
			super.positionCell(cell, position);
			LineInfo lineInfo = DefoldStyledTextSkin.this.lineInfoMap.get(cell);
			if (lineInfo != null) {
				lineInfo.setDomainElement(cell.domainElement);
				lineInfo.setLayoutY(cell.getLayoutY());
			}

		}

		@Override
		public List<LineCell> getCells() {
			return super.getCells();
		}

		@Override
		public void rebuildCells() {
			super.rebuildCells();
			DefoldStyledTextSkin.this.lineRuler.skipRelayout = true;
			Platform.runLater(new Runnable() {

				@Override
				public void run() {
					DefoldStyledTextSkin.this.lineRuler.skipRelayout = false;
					DefoldStyledTextSkin.this.lineRuler.requestLayout();
				}
			});
		}
	}
}
