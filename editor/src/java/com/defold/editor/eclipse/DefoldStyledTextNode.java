package com.defold.editor.eclipse;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

import javafx.beans.property.IntegerProperty;
import javafx.beans.property.ObjectProperty;
import javafx.beans.value.ObservableValue;
import javafx.collections.ObservableList;
import javafx.css.CssMetaData;
import javafx.css.ParsedValue;
import javafx.css.SimpleStyleableIntegerProperty;
import javafx.css.SimpleStyleableObjectProperty;
import javafx.css.StyleConverter;
import javafx.css.Styleable;
import javafx.css.StyleableProperty;
import javafx.geometry.Point2D;
import javafx.scene.Node;
import javafx.scene.layout.Region;
import javafx.scene.paint.Color;
import javafx.scene.paint.Paint;
import javafx.scene.shape.MoveTo;
import javafx.scene.shape.PathElement;
import javafx.scene.text.Font;
import javafx.scene.text.Text;
import javafx.scene.text.TextBoundsType;

import org.eclipse.fx.core.Util;
import org.eclipse.fx.ui.controls.styledtext.DecorationStrategyFactory;
import org.eclipse.jdt.annotation.NonNull;
import org.eclipse.jdt.annotation.Nullable;

import com.sun.javafx.css.converters.PaintConverter;
import com.sun.javafx.scene.text.HitInfo;

/**
 * A node who allows to decorate the text
 *
 * @since 2.0
 */
@SuppressWarnings("restriction")
public class DefoldStyledTextNode extends Region {

	/**
	 * A strategy to decorate the text
	 */
	public interface DecorationStrategy {
		/**
		 * Attach the decoration on the text
		 *
		 * @param node
		 *            the node the decoration is attached to
		 * @param textNode
		 *            the text node decorated
		 */
		public void attach(DefoldStyledTextNode node, Text textNode);

		/**
		 * Remove the decoration from the text
		 *
		 * @param node
		 *            the node the decoration is attached to
		 * @param textNode
		 *            the text node decorated
		 */
		public void unattach(DefoldStyledTextNode node, Text textNode);

		/**
		 * Layout the decoration
		 *
		 * @param node
		 *            the node the layout pass is done on
		 * @param textNode
		 *            the text node decorated
		 */
		public void layout(DefoldStyledTextNode node, Text textNode);
	}

	private final Text textNode;

	@SuppressWarnings("null")
	@NonNull
	private static final CssMetaData<DefoldStyledTextNode, @NonNull Paint> FILL = new CssMetaData<DefoldStyledTextNode, @NonNull Paint>("-fx-fill", PaintConverter.getInstance(), Color.BLACK) { //$NON-NLS-1$

		@Override
		public boolean isSettable(DefoldStyledTextNode styleable) {
			return !styleable.fillProperty().isBound();
		}

		@SuppressWarnings("unchecked")
		@Override
		public StyleableProperty<@NonNull Paint> getStyleableProperty(DefoldStyledTextNode styleable) {
			return (StyleableProperty<@NonNull Paint>) styleable.fill;
		}

	};
	@SuppressWarnings("null")
	@NonNull
	final ObjectProperty<@NonNull Paint> fill = new SimpleStyleableObjectProperty<>(FILL, this, "fill", Color.BLACK); //$NON-NLS-1$

	/**
	 * The paint used to fill the text
	 *
	 * @return the property to observe
	 */
	public final @NonNull ObjectProperty<@NonNull Paint> fillProperty() {
		return (ObjectProperty<@NonNull Paint>) this.fill;
	}

	/**
	 * @return the paint used to fill the text
	 */
	public final @NonNull Paint getFill() {
		return this.fillProperty().get();
	}

	/**
	 * Set a new paint
	 *
	 * @param color
	 *            the paint used to fill the text
	 */
	public final void setFill(final @NonNull Paint color) {
		this.fillProperty().set(color);
	}

	static class DecorationStyleConverter extends StyleConverter<ParsedValue<?, DecorationStrategy>, DecorationStrategy> {
		private static Map<String, DecorationStrategyFactory> FACTORIES;

		@SuppressWarnings("null")
		@Override
		public DecorationStrategy convert(ParsedValue<ParsedValue<?, DecorationStrategy>, DecorationStrategy> value, Font font) {
			String definition = value.getValue() + ""; //$NON-NLS-1$

			if (FACTORIES == null) {
				FACTORIES = Util.lookupServiceList(getClass(), DecorationStrategyFactory.class).stream().sorted((f1, f2) -> -1 * Integer.compare(f1.getRanking(), f2.getRanking())).collect(Collectors.toMap(f -> f.getDecorationStrategyName(), f -> f));
			}

			String type;
			if (definition.contains("(")) { //$NON-NLS-1$
				type = definition.substring(0, definition.indexOf('('));
			} else {
				type = definition + ""; //$NON-NLS-1$
			}

			DecorationStrategyFactory strategy = FACTORIES.get(type);
			if (strategy != null) {
				return (DecorationStrategy) strategy.create(definition.contains("(") ? definition.substring(definition.indexOf('(') + 1, definition.lastIndexOf(')')) : null); //$NON-NLS-1$
			}

			return null;
		}
	}

	@NonNull
	private static final DecorationStyleConverter CONVERTER = new DecorationStyleConverter();

	@SuppressWarnings("null")
	@NonNull
	private static final CssMetaData<DefoldStyledTextNode, @Nullable DecorationStrategy> DECORATIONSTRATEGY = new CssMetaData<DefoldStyledTextNode, @Nullable DecorationStrategy>("-efx-decoration", CONVERTER, null) { //$NON-NLS-1$

		@Override
		public boolean isSettable(DefoldStyledTextNode node) {
			return !node.decorationStrategyProperty().isBound();
		}

		@SuppressWarnings("unchecked")
		@Override
		public StyleableProperty<@Nullable DecorationStrategy> getStyleableProperty(DefoldStyledTextNode node) {
			return (StyleableProperty<@Nullable DecorationStrategy>) node.decorationStrategyProperty();
		}

	};

	@SuppressWarnings("null")
	@NonNull
	private static final CssMetaData<DefoldStyledTextNode, @NonNull Number> TABCHARADANCE = new CssMetaData<DefoldStyledTextNode, @NonNull Number>("-efx-tab-char-advance", StyleConverter.getSizeConverter(), Integer.valueOf(4)) { //$NON-NLS-1$

		@Override
		public boolean isSettable(DefoldStyledTextNode styleable) {
			return !styleable.tabCharAdvanceProperty().isBound();
		}

		@SuppressWarnings("unchecked")
		@Override
		public StyleableProperty<@NonNull Number> getStyleableProperty(DefoldStyledTextNode styleable) {
			return (StyleableProperty<@NonNull Number>) styleable.tabCharAdvance;
		}

	};

	private static final List<CssMetaData<? extends Styleable, ?>> STYLEABLES;

	static {
		final List<CssMetaData<? extends Styleable, ?>> styleables = new ArrayList<CssMetaData<? extends Styleable, ?>>(Region.getClassCssMetaData());
		styleables.add(DECORATIONSTRATEGY);
		styleables.add(FILL);
		STYLEABLES = Collections.unmodifiableList(styleables);
	}

	public static List<CssMetaData<? extends Styleable, ?>> getClassCssMetaData() {
		return STYLEABLES;
	}

	@Override
	public List<CssMetaData<? extends Styleable, ?>> getCssMetaData() {
		return getClassCssMetaData();
	}

	private @NonNull ObjectProperty<@Nullable DecorationStrategy> decorationStrategy = new SimpleStyleableObjectProperty<@Nullable DecorationStrategy>(DECORATIONSTRATEGY, this, "decorationStrategy"); //$NON-NLS-1$

	/**
	 * The current strategy used for decoration
	 *
	 * @return the property to observe
	 */
	public final @NonNull ObjectProperty<@Nullable DecorationStrategy> decorationStrategyProperty() {
		return this.decorationStrategy;
	}

	/**
	 * @return strategy used for decoration
	 */
	public final @Nullable DecorationStrategy getDecorationStrategy() {
		return this.decorationStrategyProperty().get();
	}

	/**
	 * Set a new strategy used for decoration
	 *
	 * @param strategy
	 *            the strategy
	 */
	public final void setDecorationStrategy(final @Nullable DecorationStrategy strategy) {
		this.decorationStrategyProperty().set(strategy);
	}

	@NonNull
	IntegerProperty tabCharAdvance = new SimpleStyleableIntegerProperty(TABCHARADANCE, this, "tabCharAdvance", Integer.valueOf(4)); //$NON-NLS-1$

	/**
	 * Number of chars to use for tab advance (default is 4)
	 *
	 * @return the property to observe
	 */
	public final IntegerProperty tabCharAdvanceProperty() {
		return this.tabCharAdvance;
	}

	/**
	 * @return the number of chars to use for tab advance (default is 4)
	 */
	public final int getTabCharAdvance() {
		return this.tabCharAdvanceProperty().get();
	}

	/**
	 * Set a new number for chars to advance for a tab
	 *
	 * @param tabCharAdvance
	 *            the number of chars to use for tab advance (default is 4)
	 */
	public final void setTabCharAdvance(final int tabCharAdvance) {
		this.tabCharAdvanceProperty().set(tabCharAdvance);
	}

	private int startOffset;
	private List<Integer> tabPositions = new ArrayList<>();
	private String originalText;

	/**
	 * Create a new styled text node
	 *
	 * @param text
	 *            the text
	 */
	public DefoldStyledTextNode(String text) {
		getStyleClass().add("styled-text-node"); //$NON-NLS-1$
		this.originalText = text;

		this.textNode = new Text(processText(text));
		this.textNode.setBoundsType(TextBoundsType.LOGICAL_VERTICAL_CENTER);
		this.textNode.fillProperty().bind(fillProperty());
		getChildren().add(this.textNode);
		this.decorationStrategy.addListener(this::handleDecorationChange);

		this.tabCharAdvance.addListener(o -> {
			this.textNode.setText(processText(text));
		});
	}

	private String processText(String text) {
		String tmp = text;
		StringBuilder b = new StringBuilder();
		for (int i = 0; i < this.tabCharAdvance.get(); i++) {
			b.append(" "); //$NON-NLS-1$
		}
		int position = -1;
		this.tabPositions.clear();
		while ((position = tmp.indexOf('\t')) != -1) {
			tmp = tmp.replaceFirst("\t", b.toString()); //$NON-NLS-1$
			this.tabPositions.add(Integer.valueOf(position));
		}
		return tmp;
	}

	private void handleDecorationChange(ObservableValue<? extends DecorationStrategy> observable, DecorationStrategy oldValue, DecorationStrategy newValue) {
		if (oldValue != null) {
			oldValue.unattach(this, this.textNode);
		}

		if (newValue != null) {
			newValue.attach(this, this.textNode);
		}
	}

	void setStartOffset(int startOffset) {
		this.startOffset = startOffset;
	}

	/**
	 * @return the start offset in the file
	 */
	public int getStartOffset() {
		return this.startOffset;
	}

	/**
	 * @return the end offset in the file
	 */
	public int getEndOffset() {
		return this.startOffset + getText().length();
	}

	/**
	 * Check if the text node intersects with the start and end locations
	 *
	 * @param start
	 *            the start
	 * @param end
	 *            the end
	 * @return <code>true</code> if intersects
	 */
	public boolean intersectOffset(int start, int end) {
		if (getStartOffset() > end) {
			return false;
		} else if (getEndOffset() < start) {
			return false;
		}
		return true;
	}

	@Override
	public String toString() {
		return this.originalText;
	}

	@Override
	public ObservableList<Node> getChildren() {
		return super.getChildren();
	}

	/**
	 * @return the text
	 */
	public String getText() {
		return this.originalText;
	}

	@Override
	protected void layoutChildren() {
		super.layoutChildren();
		this.textNode.relocate(0, 0);
		DecorationStrategy decorationStrategy2 = this.decorationStrategy.get();
		if (decorationStrategy2 != null) {
			decorationStrategy2.layout(this, this.textNode);
		}

	}

	/**
	 * Get the caret index at the given point
	 *
	 * @param point
	 *            the point relative to this node
	 * @return the index or <code>-1</code>
	 */
	public int getCaretIndexAtPoint(Point2D point) {
		@SuppressWarnings("deprecation")
		HitInfo info = this.textNode.impl_hitTestChar(this.textNode.sceneToLocal(localToScene(point)));
		if (info != null) {
			int idx = info.getInsertionIndex();
			int toRemove = 0;
			for (Integer i : this.tabPositions) {
				if (i.intValue() <= idx && idx < i.intValue() + this.tabCharAdvance.get()) {
					toRemove += idx - i.intValue();
					// If we are in the 2nd half of the tab we
					// simply move one past the value
					if ((idx - i.intValue()) % this.tabCharAdvance.get() >= this.tabCharAdvance.get() / 2) {
						idx += 1;
					}
					break;
				} else if (i.intValue() < idx) {
					toRemove += this.tabCharAdvance.get() - 1;
				}
			}
			idx -= toRemove;
			return idx;
		}
		return -1;
	}

	/**
	 * Find the x coordinate where we the character for the given index starts
	 *
	 * @param index
	 *            the index
	 * @return the location or <code>0</code> if not found
	 */
	@SuppressWarnings("deprecation")
	public double getCharLocation(int index) {
		int realIndex = index;
		for (Integer i : this.tabPositions) {
			if (i.intValue() < realIndex) {
				realIndex += this.tabCharAdvance.get() - 1;
			}
		}
		this.textNode.setImpl_caretPosition(realIndex);
		PathElement[] pathElements = this.textNode.getImpl_caretShape();
		for (PathElement e : pathElements) {
			if (e instanceof MoveTo) {
				return this.textNode.localToParent(((MoveTo) e).getX(), 0).getX();
			}
		}

		return 0.0;
	}
}
