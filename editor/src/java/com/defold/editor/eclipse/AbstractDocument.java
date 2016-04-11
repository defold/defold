package com.defold.editor.eclipse;

import java.util.AbstractList;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.regex.PatternSyntaxException;

import org.eclipse.jface.text.BadLocationException;
import org.eclipse.jface.text.BadPositionCategoryException;
import org.eclipse.jface.text.BadPartitioningException;
import org.eclipse.jface.text.DefaultPositionUpdater;
import org.eclipse.jface.text.DocumentRewriteSessionType;
import org.eclipse.jface.text.DocumentRewriteSession;
import org.eclipse.jface.text.DocumentRewriteSessionEvent;
import org.eclipse.jface.text.IDocument;
import org.eclipse.jface.text.IDocumentExtension;
import org.eclipse.jface.text.IDocumentExtension2;
import org.eclipse.jface.text.IDocumentExtension3;
import org.eclipse.jface.text.IDocumentExtension4;
import org.eclipse.jface.text.IRepairableDocument;
import org.eclipse.jface.text.IRepairableDocumentExtension;
import org.eclipse.jface.text.ITextStore;
import org.eclipse.jface.text.ILineTracker;
import org.eclipse.jface.text.ILineTrackerExtension;
import org.eclipse.jface.text.Position;
import org.eclipse.jface.text.IPositionUpdater;
import org.eclipse.jface.text.IRegion;
import org.eclipse.jface.text.ITypedRegion;
import org.eclipse.jface.text.DocumentEvent;
import org.eclipse.jface.text.IDocumentPartitioner;
import org.eclipse.jface.text.IDocumentPartitionerExtension;
import org.eclipse.jface.text.IDocumentPartitionerExtension2;
import org.eclipse.jface.text.IDocumentPartitionerExtension3;
import org.eclipse.jface.text.IDocumentPartitioningListener;
import org.eclipse.jface.text.IDocumentPartitioningListenerExtension;
import org.eclipse.jface.text.IDocumentPartitioningListenerExtension2;
import org.eclipse.jface.text.IDocumentListener;
import org.eclipse.jface.text.DocumentPartitioningChangedEvent;
import org.eclipse.jface.text.FindReplaceDocumentAdapter;
import org.eclipse.jface.text.IDocumentRewriteSessionListener;
import org.eclipse.jface.text.TypedRegion;

//SafeRunner has OSGI deps
//import org.eclipse.core.runtime.ISafeRunnable;
import org.eclipse.core.runtime.Assert;
import org.eclipse.core.runtime.ListenerList;
//import org.eclipse.core.runtime.SafeRunner;


/**
 * Abstract default implementation of <code>IDocument</code> and its extension
 * interfaces {@link org.eclipse.jface.text.IDocumentExtension},
 * {@link org.eclipse.jface.text.IDocumentExtension2},
 * {@link org.eclipse.jface.text.IDocumentExtension3},
 * {@link org.eclipse.jface.text.IDocumentExtension4}, as well as
 * {@link org.eclipse.jface.text.IRepairableDocument}.
 * <p>
 *
 * An <code>AbstractDocument</code> supports the following implementation
 * plug-ins:
 * <ul>
 * <li>a text store implementing {@link org.eclipse.jface.text.ITextStore} for
 *     storing and managing the document's content,</li>
 * <li>a line tracker implementing {@link org.eclipse.jface.text.ILineTracker}
 *     to map character positions to line numbers and vice versa</li>
 * </ul>
 * The document can dynamically change the text store when switching between
 * sequential rewrite mode and normal mode.
 * <p>
 *
 * This class must be subclassed. Subclasses must configure which implementation
 * plug-ins the document instance should use. Subclasses are not intended to
 * overwrite existing methods.
 *
 * @see org.eclipse.jface.text.ITextStore
 * @see org.eclipse.jface.text.ILineTracker
 */
public abstract class AbstractDocument implements IDocument, IDocumentExtension, IDocumentExtension2, IDocumentExtension3, IDocumentExtension4, IRepairableDocument, IRepairableDocumentExtension {

	/**
	 * Tells whether this class is in debug mode.
	 * @since 3.1
	 */
	private static final boolean DEBUG= false;


	/**
	 * Inner class to bundle a registered post notification replace operation together with its
	 * owner.
	 *
	 * @since 2.0
	 */
	static private class RegisteredReplace {
		/** The owner of this replace operation. */
		IDocumentListener fOwner;
		/** The replace operation */
		IDocumentExtension.IReplace fReplace;

		/**
		 * Creates a new bundle object.
		 * @param owner the document listener owning the replace operation
		 * @param replace the replace operation
		 */
		RegisteredReplace(IDocumentListener owner, IDocumentExtension.IReplace replace) {
			fOwner= owner;
			fReplace= replace;
		}
	}


	/** The document's text store */
	private ITextStore   fStore;
	/** The document's line tracker */
	private ILineTracker fTracker;
	/** The registered document listeners */
	private ListenerList fDocumentListeners;
	/** The registered pre-notified document listeners */
	private ListenerList fPrenotifiedDocumentListeners;
	/** The registered document partitioning listeners */
	private ListenerList fDocumentPartitioningListeners;
	/** All positions managed by the document ordered by their start positions. */
	private Map<String, List<Position>> fPositions;
	/**
	 * All positions managed by the document ordered by their end positions.
	 * @since 3.4
	 */
	private Map<String, List<Position>> fEndPositions;
	/** All registered document position updaters */
	private List<IPositionUpdater> fPositionUpdaters;
	/**
	 * The list of post notification changes
	 * @since 2.0
	 */
	private List<RegisteredReplace> fPostNotificationChanges;
	/**
	 * The reentrance count for post notification changes.
	 * @since 2.0
	 */
	private int fReentranceCount= 0;
	/**
	 * Indicates whether post notification change processing has been stopped.
	 * @since 2.0
	 */
	private int fStoppedCount= 0;
	/**
	 * Indicates whether the registration of post notification changes should be ignored.
	 * @since 2.1
	 */
	private boolean fAcceptPostNotificationReplaces= true;
	/**
	 * Indicates whether the notification of listeners has been stopped.
	 * @since 2.1
	 */
	private int fStoppedListenerNotification= 0;
	/**
	 * The document event to be sent after listener notification has been resumed.
	 * @since 2.1
	 */
	private DocumentEvent fDeferredDocumentEvent;
	/**
	 * The registered document partitioners.
	 * @since 3.0
	 */
	private Map<String, IDocumentPartitioner> fDocumentPartitioners;
	/**
	 * The partitioning changed event.
	 * @since 3.0
	 */
	private DocumentPartitioningChangedEvent fDocumentPartitioningChangedEvent;
	/**
	 * The find/replace document adapter.
	 * @since 3.0
	 */
	private FindReplaceDocumentAdapter fFindReplaceDocumentAdapter;
	/**
	 * The active document rewrite session.
	 * @since 3.1
	 */
	private DocumentRewriteSession fDocumentRewriteSession;
	/**
	 * The registered document rewrite session listeners.
	 * @since 3.1
	 */
	private List<IDocumentRewriteSessionListener> fDocumentRewriteSessionListeners;
	/**
	 * The current modification stamp.
	 * @since 3.1
	 */
	private long fModificationStamp= IDocumentExtension4.UNKNOWN_MODIFICATION_STAMP;
	/**
	 * Keeps track of next modification stamp.
	 * @since 3.1.1
	 */
	private long fNextModificationStamp= IDocumentExtension4.UNKNOWN_MODIFICATION_STAMP;
	/**
	 * This document's default line delimiter.
	 * @since 3.1
	 */
	private String fInitialLineDelimiter;


	/**
	 * The default constructor does not perform any configuration
	 * but leaves it to the clients who must first initialize the
	 * implementation plug-ins and then call <code>completeInitialization</code>.
	 * Results in the construction of an empty document.
	 */
	protected AbstractDocument() {
		fModificationStamp= getNextModificationStamp();
	}


	/**
	 * Returns the document's text store. Assumes that the
	 * document has been initialized with a text store.
	 *
	 * @return the document's text store
	 */
	protected ITextStore getStore() {
		Assert.isNotNull(fStore);
		return fStore;
	}

	/**
	 * Returns the document's line tracker. Assumes that the
	 * document has been initialized with a line tracker.
	 *
	 * @return the document's line tracker
	 */
	protected ILineTracker getTracker() {
		Assert.isNotNull(fTracker);
		return fTracker;
	}

	private static <T> List<T> asList(Object[] listeners) {
		// Workaround for Bug 483340: ListenerList should be parameterized
		// Use Arrays.asList(..) once that bug is fixed.
		return new AbstractList<T>() {
			@SuppressWarnings("unchecked")
			@Override
			public T get(int index) {
				return (T) listeners[index];
			}

			@Override
			public int size() {
				return listeners.length;
			}
		};
	}


	/**
	 * Returns the document's document listeners.
	 *
	 * @return the document's document listeners
	 */
	protected List<IDocumentListener> getDocumentListeners() {
		return asList(fDocumentListeners.getListeners());
	}

	/**
	 * Returns the document's partitioning listeners.
	 *
	 * @return the document's partitioning listeners
	 */
	protected List<IDocumentPartitioningListener> getDocumentPartitioningListeners() {
		return asList(fDocumentPartitioningListeners.getListeners());
	}

	/**
	 * Returns all positions managed by the document grouped by category.
	 *
	 * @return the document's positions
     */
	protected Map<String, List<Position>> getDocumentManagedPositions() {
		return fPositions;
	}

	@Override
	public IDocumentPartitioner getDocumentPartitioner() {
		return getDocumentPartitioner(DEFAULT_PARTITIONING);
	}



	//--- implementation configuration interface ------------

	/**
	 * Sets the document's text store.
	 * Must be called at the beginning of the constructor.
	 *
	 * @param store the document's text store
	 */
	protected void setTextStore(ITextStore store) {
		fStore= store;
	}

	/**
	 * Sets the document's line tracker.
	 * Must be called at the beginning of the constructor.
	 *
	 * @param tracker the document's line tracker
	 */
	protected void setLineTracker(ILineTracker tracker) {
		fTracker= tracker;
	}

	@Override
	public void setDocumentPartitioner(IDocumentPartitioner partitioner) {
		setDocumentPartitioner(DEFAULT_PARTITIONING, partitioner);
	}

	/**
	 * Initializes document listeners, positions, and position updaters.
	 * Must be called inside the constructor after the implementation plug-ins
	 * have been set.
	 */
	protected void completeInitialization() {

		fPositions= new HashMap<>();
		fEndPositions= new HashMap<>();
		fPositionUpdaters= new ArrayList<>();
		fDocumentListeners= new ListenerList(ListenerList.IDENTITY);
		fPrenotifiedDocumentListeners= new ListenerList(ListenerList.IDENTITY);
		fDocumentPartitioningListeners= new ListenerList(ListenerList.IDENTITY);
		fDocumentRewriteSessionListeners= new ArrayList<>();

		addPositionCategory(DEFAULT_CATEGORY);
		addPositionUpdater(new DefaultPositionUpdater(DEFAULT_CATEGORY));
	}


	//-------------------------------------------------------

	@Override
	public void addDocumentListener(IDocumentListener listener) {
		Assert.isNotNull(listener);
		fDocumentListeners.add(listener);
	}

	@Override
	public void removeDocumentListener(IDocumentListener listener) {
		Assert.isNotNull(listener);
		fDocumentListeners.remove(listener);
	}

	@Override
	public void addPrenotifiedDocumentListener(IDocumentListener listener) {
		Assert.isNotNull(listener);
		fPrenotifiedDocumentListeners.add(listener);
	}

	@Override
	public void removePrenotifiedDocumentListener(IDocumentListener listener) {
		Assert.isNotNull(listener);
		fPrenotifiedDocumentListeners.remove(listener);
	}

	@Override
	public void addDocumentPartitioningListener(IDocumentPartitioningListener listener) {
		Assert.isNotNull(listener);
		fDocumentPartitioningListeners.add(listener);
	}

	@Override
	public void removeDocumentPartitioningListener(IDocumentPartitioningListener listener) {
		Assert.isNotNull(listener);
		fDocumentPartitioningListeners.remove(listener);
	}

	@Override
	public void addPosition(String category, Position position) throws BadLocationException, BadPositionCategoryException  {

		if ((0 > position.offset) || (0 > position.length) || (position.offset + position.length > getLength()))
			throw new BadLocationException();

		if (category == null)
			throw new BadPositionCategoryException();

		List<Position> list= fPositions.get(category);
		if (list == null)
			throw new BadPositionCategoryException();
		list.add(computeIndexInPositionList(list, position.offset), position);

		List<Position> endPositions= fEndPositions.get(category);
		if (endPositions == null)
			throw new BadPositionCategoryException();
		endPositions.add(computeIndexInPositionList(endPositions, position.offset + position.length - 1, false), position);
	}

	@Override
	public void addPosition(Position position) throws BadLocationException {
		try {
			addPosition(DEFAULT_CATEGORY, position);
		} catch (BadPositionCategoryException e) {
		}
	}

	@Override
	public void addPositionCategory(String category) {

		if (category == null)
			return;

		if (!containsPositionCategory(category)) {
			fPositions.put(category, new ArrayList<>());
			fEndPositions.put(category, new ArrayList<>());
		}
	}

	@Override
	public void addPositionUpdater(IPositionUpdater updater) {
		insertPositionUpdater(updater, fPositionUpdaters.size());
	}

	@Override
	public boolean containsPosition(String category, int offset, int length) {

		if (category == null)
			return false;

		List<Position> list= fPositions.get(category);
		if (list == null)
			return false;

		int size= list.size();
		if (size == 0)
			return false;

		int index= computeIndexInPositionList(list, offset);
		if (index < size) {
			Position p= list.get(index);
			while (p != null && p.offset == offset) {
				if (p.length == length)
					return true;
				++ index;
				p= (index < size) ? list.get(index) : null;
			}
		}

		return false;
	}

	@Override
	public boolean containsPositionCategory(String category) {
		if (category != null)
			return fPositions.containsKey(category);
		return false;
	}


	/**
	 * Computes the index in the list of positions at which a position with the given
	 * offset would be inserted. The position is supposed to become the first in this list
	 * of all positions with the same offset.
	 *
	 * @param positions the list in which the index is computed
	 * @param offset the offset for which the index is computed
	 * @return the computed index
	 *
	 * @see IDocument#computeIndexInCategory(String, int)
	 * @deprecated As of 3.4, replaced by {@link #computeIndexInPositionList(List, int, boolean)}
	 */
	@Deprecated
	protected int computeIndexInPositionList(List<? extends Position> positions, int offset) {
		return computeIndexInPositionList(positions, offset, true);
	}

	/**
	 * Computes the index in the list of positions at which a position with the given
	 * position would be inserted. The position to insert is supposed to become the first
	 * in this list of all positions with the same position.
	 *
	 * @param positions the list in which the index is computed
	 * @param offset the offset for which the index is computed
	 * @param orderedByOffset <code>true</code> if ordered by offset, false if ordered by end position
	 * @return the computed index
	 * @since 3.4
	 */
	protected int computeIndexInPositionList(List<? extends Position> positions, int offset, boolean orderedByOffset) {
		if (positions.size() == 0)
			return 0;

		int left= 0;
		int right= positions.size() -1;
		int mid= 0;
		Position p= null;

		while (left < right) {

			mid= (left + right) / 2;

			p= positions.get(mid);
			int pOffset= getOffset(orderedByOffset, p);
			if (offset < pOffset) {
				if (left == mid)
					right= left;
				else
					right= mid -1;
			} else if (offset > pOffset) {
				if (right == mid)
					left= right;
				else
					left= mid  +1;
			} else if (offset == pOffset) {
				left= right= mid;
			}

		}

		int pos= left;
		p= positions.get(pos);
		int pPosition= getOffset(orderedByOffset, p);
		if (offset > pPosition) {
			// append to the end
			pos++;
		} else {
			// entry will become the first of all entries with the same offset
			do {
				--pos;
				if (pos < 0)
					break;
				p= positions.get(pos);
				pPosition= getOffset(orderedByOffset, p);
			} while (offset == pPosition);
			++pos;
		}

		Assert.isTrue(0 <= pos && pos <= positions.size());

		return pos;
	}

	/*
	 * @since 3.4
	 */
	private int getOffset(boolean orderedByOffset, Position position) {
		if (orderedByOffset || position.getLength() == 0)
			return position.getOffset();
		return position.getOffset() + position.getLength() - 1;
	}

	@Override
	public int computeIndexInCategory(String category, int offset) throws BadLocationException, BadPositionCategoryException {

		if (0 > offset || offset > getLength())
			throw new BadLocationException();

		List<Position> c= fPositions.get(category);
		if (c == null)
			throw new BadPositionCategoryException();

		return computeIndexInPositionList(c, offset);
	}

	/**
	 * Fires the document partitioning changed notification to all registered
	 * document partitioning listeners. Uses a robust iterator.
	 *
	 * @deprecated as of 2.0. Use <code>fireDocumentPartitioningChanged(IRegion)</code> instead.
	 */
	@Deprecated
	protected void fireDocumentPartitioningChanged() {
		if (fDocumentPartitioningListeners == null)
			return;

		Object[] listeners= fDocumentPartitioningListeners.getListeners();
		for (int i= 0; i < listeners.length; i++)
			((IDocumentPartitioningListener)listeners[i]).documentPartitioningChanged(this);
	}

	/**
	 * Fires the document partitioning changed notification to all registered
	 * document partitioning listeners. Uses a robust iterator.
	 *
	 * @param region the region in which partitioning has changed
	 *
	 * @see IDocumentPartitioningListenerExtension
	 * @since 2.0
	 * @deprecated as of 3.0. Use
	 *             <code>fireDocumentPartitioningChanged(DocumentPartitioningChangedEvent)</code>
	 *             instead.
	 */
	@Deprecated
	protected void fireDocumentPartitioningChanged(IRegion region) {
		if (fDocumentPartitioningListeners == null)
			return;

		Object[] listeners= fDocumentPartitioningListeners.getListeners();
		for (int i= 0; i < listeners.length; i++) {
			IDocumentPartitioningListener l= (IDocumentPartitioningListener)listeners[i];
			try {
				if (l instanceof IDocumentPartitioningListenerExtension)
					((IDocumentPartitioningListenerExtension)l).documentPartitioningChanged(this, region);
				else
					l.documentPartitioningChanged(this);
			} catch (Exception ex) {
				log(ex);
			}
		}
	}

	/**
	 * Fires the document partitioning changed notification to all registered
	 * document partitioning listeners. Uses a robust iterator.
	 *
	 * @param event the document partitioning changed event
	 *
	 * @see IDocumentPartitioningListenerExtension2
	 * @since 3.0
	 */
	protected void fireDocumentPartitioningChanged(DocumentPartitioningChangedEvent event) {
		if (fDocumentPartitioningListeners == null)
			return;

		Object[] listeners= fDocumentPartitioningListeners.getListeners();
		for (int i= 0; i < listeners.length; i++) {
			IDocumentPartitioningListener l= (IDocumentPartitioningListener)listeners[i];
			try {
				if (l instanceof IDocumentPartitioningListenerExtension2) {
					IDocumentPartitioningListenerExtension2 extension2= (IDocumentPartitioningListenerExtension2)l;
					extension2.documentPartitioningChanged(event);
				} else if (l instanceof IDocumentPartitioningListenerExtension) {
					IDocumentPartitioningListenerExtension extension= (IDocumentPartitioningListenerExtension)l;
					extension.documentPartitioningChanged(this, event.getCoverage());
				} else {
					l.documentPartitioningChanged(this);
				}
			} catch (Exception ex) {
				log(ex);
			}
		}
	}

	/**
	 * Fires the given document event to all registers document listeners informing them
	 * about the forthcoming document manipulation. Uses a robust iterator.
	 *
	 * @param event the event to be sent out
	 */
	protected void fireDocumentAboutToBeChanged(DocumentEvent event) {

		// IDocumentExtension
		if (fReentranceCount == 0)
			flushPostNotificationChanges();

		if (fDocumentPartitioners != null) {
			Iterator<IDocumentPartitioner> e= fDocumentPartitioners.values().iterator();
			while (e.hasNext()) {
				IDocumentPartitioner p= e.next();
				if (p instanceof IDocumentPartitionerExtension3) {
					IDocumentPartitionerExtension3 extension= (IDocumentPartitionerExtension3) p;
					if (extension.getActiveRewriteSession() != null)
						continue;
				}
				try {
					p.documentAboutToBeChanged(event);
				} catch (Exception ex) {
					log(ex);
				}
			}
		}

		Object[] listeners= fPrenotifiedDocumentListeners.getListeners();
		for (int i= 0; i < listeners.length; i++) {
			try {
				((IDocumentListener)listeners[i]).documentAboutToBeChanged(event);
			} catch (Exception ex) {
				log(ex);
			}
		}

		listeners= fDocumentListeners.getListeners();
		for (int i= 0; i < listeners.length; i++) {
			try {
				((IDocumentListener)listeners[i]).documentAboutToBeChanged(event);
			} catch (Exception ex) {
				log(ex);
			}
		}

	}

	/**
	 * Updates document partitioning and document positions according to the
	 * specification given by the document event.
	 *
	 * @param event the document event describing the change to which structures must be adapted
	 */
	protected void updateDocumentStructures(DocumentEvent event) {

		if (fDocumentPartitioners != null) {
			fDocumentPartitioningChangedEvent= new DocumentPartitioningChangedEvent(this);
			Iterator<String> e= fDocumentPartitioners.keySet().iterator();
			while (e.hasNext()) {
				String partitioning= e.next();
				IDocumentPartitioner partitioner= fDocumentPartitioners.get(partitioning);

				if (partitioner instanceof IDocumentPartitionerExtension3) {
					IDocumentPartitionerExtension3 extension= (IDocumentPartitionerExtension3) partitioner;
					if (extension.getActiveRewriteSession() != null)
						continue;
				}

				if (partitioner instanceof IDocumentPartitionerExtension) {
					IDocumentPartitionerExtension extension= (IDocumentPartitionerExtension) partitioner;
					IRegion r= extension.documentChanged2(event);
					if (r != null)
						fDocumentPartitioningChangedEvent.setPartitionChange(partitioning, r.getOffset(), r.getLength());
				} else {
					if (partitioner.documentChanged(event))
						fDocumentPartitioningChangedEvent.setPartitionChange(partitioning, 0, event.getDocument().getLength());
				}
			}
		}

		if (fPositions.size() > 0)
			updatePositions(event);
	}

	/**
	 * Notifies all listeners about the given document change. Uses a robust
	 * iterator.
	 * <p>
	 * Executes all registered post notification replace operation.
	 *
	 * @param event the event to be sent out.
	 */
	protected void doFireDocumentChanged(DocumentEvent event) {
		boolean changed= fDocumentPartitioningChangedEvent != null && !fDocumentPartitioningChangedEvent.isEmpty();
		IRegion change= changed ? fDocumentPartitioningChangedEvent.getCoverage() : null;
		doFireDocumentChanged(event, changed, change);
	}

	/**
	 * Notifies all listeners about the given document change.
	 * Uses a robust iterator. <p>
	 * Executes all registered post notification replace operation.
	 *
	 * @param event the event to be sent out
	 * @param firePartitionChange <code>true</code> if a partition change notification should be sent
	 * @param partitionChange the region whose partitioning changed
	 * @since 2.0
	 * @deprecated as of 3.0. Use <code>doFireDocumentChanged2(DocumentEvent)</code> instead; this method will be removed.
	 */
	@Deprecated
	protected void doFireDocumentChanged(DocumentEvent event, boolean firePartitionChange, IRegion partitionChange) {
		doFireDocumentChanged2(event);
	}

	/**
	 * Notifies all listeners about the given document change. Uses a robust
	 * iterator.
	 * <p>
	 * Executes all registered post notification replace operation.
	 * <p>
	 * This method will be renamed to <code>doFireDocumentChanged</code>.
	 *
	 * @param event the event to be sent out
	 * @since 3.0
	 */
	protected void doFireDocumentChanged2(DocumentEvent event) {

		DocumentPartitioningChangedEvent p= fDocumentPartitioningChangedEvent;
		fDocumentPartitioningChangedEvent= null;
		if (p != null && !p.isEmpty())
			fireDocumentPartitioningChanged(p);

		Object[] listeners= fPrenotifiedDocumentListeners.getListeners();
		for (int i= 0; i < listeners.length; i++) {
			try {
				((IDocumentListener)listeners[i]).documentChanged(event);
			} catch (Exception ex) {
				log(ex);
			}
		}

		listeners= fDocumentListeners.getListeners();
		for (int i= 0; i < listeners.length; i++) {
			try {
				((IDocumentListener)listeners[i]).documentChanged(event);
			} catch (Exception ex) {
				log(ex);
			}
		}

		// IDocumentExtension
		++ fReentranceCount;
		try {
			if (fReentranceCount == 1)
				executePostNotificationChanges();
		} finally {
			-- fReentranceCount;
		}
	}

	/**
	 * Updates the internal document structures and informs all document listeners
	 * if listener notification has been enabled. Otherwise it remembers the event
	 * to be sent to the listeners on resume.
	 *
	 * @param event the document event to be sent out
	 */
	protected void fireDocumentChanged(DocumentEvent event) {
		updateDocumentStructures(event);

		if (fStoppedListenerNotification == 0)
			doFireDocumentChanged(event);
		else
			fDeferredDocumentEvent= event;
	}

	@Override
	public char getChar(int pos) throws BadLocationException {
		if ((0 > pos) || (pos >= getLength()))
			throw new BadLocationException();
		return getStore().get(pos);
	}

	@Override
	public String getContentType(int offset) throws BadLocationException {
		String contentType= null;
		try {
			contentType= getContentType(DEFAULT_PARTITIONING, offset, false);
			Assert.isNotNull(contentType);
		} catch (BadPartitioningException e) {
			Assert.isTrue(false);
		}
		return contentType;
	}

	@Override
	public String[] getLegalContentTypes() {
		String[] contentTypes= null;
		try {
			contentTypes= getLegalContentTypes(DEFAULT_PARTITIONING);
			Assert.isNotNull(contentTypes);
		} catch (BadPartitioningException e) {
			Assert.isTrue(false);
		}
		return contentTypes;
	}

	@Override
	public int getLength() {
		return getStore().getLength();
	}

	@Override
	public String getLineDelimiter(int line) throws BadLocationException {
		return getTracker().getLineDelimiter(line);
	}

	@Override
	public String[] getLegalLineDelimiters() {
		return getTracker().getLegalLineDelimiters();
	}

	@Override
	public String getDefaultLineDelimiter() {

		String lineDelimiter= null;

		try {
			lineDelimiter= getLineDelimiter(0);
		} catch (BadLocationException x) {
		}

		if (lineDelimiter != null)
			return lineDelimiter;

		if (fInitialLineDelimiter != null)
			return fInitialLineDelimiter;

		String sysLineDelimiter= System.getProperty("line.separator"); //$NON-NLS-1$
		String[] delimiters= getLegalLineDelimiters();
		Assert.isTrue(delimiters.length > 0);
		for (int i= 0; i < delimiters.length; i++) {
			if (delimiters[i].equals(sysLineDelimiter)) {
				lineDelimiter= sysLineDelimiter;
				break;
			}
		}

		if (lineDelimiter == null)
			lineDelimiter= delimiters[0];

		return lineDelimiter;

	}

	@Override
	public void setInitialLineDelimiter(String lineDelimiter) {
		Assert.isNotNull(lineDelimiter);
		fInitialLineDelimiter= lineDelimiter;
	}

	@Override
	public int getLineLength(int line) throws BadLocationException {
		return getTracker().getLineLength(line);
	}

	@Override
	public int getLineOfOffset(int pos) throws BadLocationException {
		return getTracker().getLineNumberOfOffset(pos);
	}

	@Override
	public int getLineOffset(int line) throws BadLocationException {
		return getTracker().getLineOffset(line);
	}

	@Override
	public IRegion getLineInformation(int line) throws BadLocationException {
		return getTracker().getLineInformation(line);
	}

	@Override
	public IRegion getLineInformationOfOffset(int offset) throws BadLocationException {
		return getTracker().getLineInformationOfOffset(offset);
	}

	@Override
	public int getNumberOfLines() {
		return getTracker().getNumberOfLines();
	}

	@Override
	public int getNumberOfLines(int offset, int length) throws BadLocationException {
		return getTracker().getNumberOfLines(offset, length);
	}

	@Override
	public int computeNumberOfLines(String text) {
		return getTracker().computeNumberOfLines(text);
	}

	@Override
	public ITypedRegion getPartition(int offset) throws BadLocationException {
		ITypedRegion partition= null;
		try {
			partition= getPartition(DEFAULT_PARTITIONING, offset, false);
			Assert.isNotNull(partition);
		} catch (BadPartitioningException e) {
			Assert.isTrue(false);
		}
		return  partition;
	}

	@Override
	public ITypedRegion[] computePartitioning(int offset, int length) throws BadLocationException {
		ITypedRegion[] partitioning= null;
		try {
			partitioning= computePartitioning(DEFAULT_PARTITIONING, offset, length, false);
			Assert.isNotNull(partitioning);
		} catch (BadPartitioningException e) {
			Assert.isTrue(false);
		}
		return partitioning;
	}

	@Override
	public Position[] getPositions(String category) throws BadPositionCategoryException {

		if (category == null)
			throw new BadPositionCategoryException();

		List<Position> c= fPositions.get(category);
		if (c == null)
			throw new BadPositionCategoryException();

		Position[] positions= new Position[c.size()];
		c.toArray(positions);
		return positions;
	}

	@Override
	public String[] getPositionCategories() {
		String[] categories= new String[fPositions.size()];
		Iterator<String> keys= fPositions.keySet().iterator();
		for (int i= 0; i < categories.length; i++)
			categories[i]= keys.next();
		return categories;
	}

	@Override
	public IPositionUpdater[] getPositionUpdaters() {
		IPositionUpdater[] updaters= new IPositionUpdater[fPositionUpdaters.size()];
		fPositionUpdaters.toArray(updaters);
		return updaters;
	}

	@Override
	public String get() {
		return getStore().get(0, getLength());
	}

	@Override
	public String get(int pos, int length) throws BadLocationException {
		int myLength= getLength();
		if ((0 > pos) || (0 > length) || (pos + length > myLength))
			throw new BadLocationException();
		return getStore().get(pos, length);
	}

	@Override
	public void insertPositionUpdater(IPositionUpdater updater, int index) {

		for (int i= fPositionUpdaters.size() - 1; i >= 0; i--) {
			if (fPositionUpdaters.get(i) == updater)
				return;
		}

		if (index == fPositionUpdaters.size())
			fPositionUpdaters.add(updater);
		else
			fPositionUpdaters.add(index, updater);
	}

	@Override
	public void removePosition(String category, Position position) throws BadPositionCategoryException {

		if (position == null)
			return;

		if (category == null)
			throw new BadPositionCategoryException();

		List<Position> c= fPositions.get(category);
		if (c == null)
			throw new BadPositionCategoryException();
		removeFromPositionsList(c, position, true);

		List<Position> endPositions= fEndPositions.get(category);
		if (endPositions == null)
			throw new BadPositionCategoryException();
		removeFromPositionsList(endPositions, position, false);
	}

	/**
	 * Remove the given position form the given list of positions based on identity not equality.
	 *
	 * @param positions a list of positions
	 * @param position the position to remove
	 * @param orderedByOffset true if <code>positions</code> is ordered by offset, false if ordered by end position
	 * @since 3.4
	 */
	private void removeFromPositionsList(List<Position> positions, Position position, boolean orderedByOffset) {
		int size= positions.size();

		//Assume position is somewhere near it was before
		int index= computeIndexInPositionList(positions, orderedByOffset ? position.offset : position.offset + position.length - 1, orderedByOffset);
		if (index < size && positions.get(index) == position) {
			positions.remove(index);
			return;
		}

		int back= index - 1;
		int forth= index + 1;
		while (back >= 0 || forth < size) {
			if (back >= 0) {
				if (position == positions.get(back)) {
					positions.remove(back);
					return;
				}
				back--;
			}

			if (forth < size) {
				if (position == positions.get(forth)) {
					positions.remove(forth);
					return;
				}
				forth++;
			}
		}
	}

	@Override
	public void removePosition(Position position) {
		try {
			removePosition(DEFAULT_CATEGORY, position);
		} catch (BadPositionCategoryException e) {
		}
	}

	@Override
	public void removePositionCategory(String category) throws BadPositionCategoryException {

		if (category == null)
			return;

		if ( !containsPositionCategory(category))
			throw new BadPositionCategoryException();

		fPositions.remove(category);
		fEndPositions.remove(category);
	}

	@Override
	public void removePositionUpdater(IPositionUpdater updater) {
		for (int i= fPositionUpdaters.size() - 1; i >= 0; i--) {
			if (fPositionUpdaters.get(i) == updater) {
				fPositionUpdaters.remove(i);
				return;
			}
		}
	}

	private long getNextModificationStamp() {
		if (fNextModificationStamp == Long.MAX_VALUE || fNextModificationStamp == IDocumentExtension4.UNKNOWN_MODIFICATION_STAMP)
			fNextModificationStamp= 0;
		else
			fNextModificationStamp= fNextModificationStamp + 1;

		return fNextModificationStamp;
	}

	@Override
	public long getModificationStamp() {
		return fModificationStamp;
	}

	@Override
	public void replace(int pos, int length, String text, long modificationStamp) throws BadLocationException {
		if ((0 > pos) || (0 > length) || (pos + length > getLength()))
			throw new BadLocationException();

		DocumentEvent e= new DocumentEvent(this, pos, length, text);
		fireDocumentAboutToBeChanged(e);

		getStore().replace(pos, length, text);
		getTracker().replace(pos, length, text);

		fModificationStamp= modificationStamp;
		fNextModificationStamp= Math.max(fModificationStamp, fNextModificationStamp);
		e.fModificationStamp= fModificationStamp;

		fireDocumentChanged(e);
	}

	/**
	 * {@inheritDoc}
	 *
	 * @since 3.4
	 */
	@Override
	public boolean isLineInformationRepairNeeded(int offset, int length, String text) throws BadLocationException {
		return false;
	}

	@Override
	public void replace(int pos, int length, String text) throws BadLocationException {
		if (length == 0 && (text == null || text.length() == 0))
			replace(pos, length, text, getModificationStamp());
		else
			replace(pos, length, text, getNextModificationStamp());
	}

	@Override
	public void set(String text) {
		set(text, getNextModificationStamp());
	}

	@Override
	public void set(String text, long modificationStamp) {
		int length= getStore().getLength();

		DocumentEvent e= new DocumentEvent(this, 0, length, text);
		fireDocumentAboutToBeChanged(e);

		getStore().set(text);
		getTracker().set(text);

		fModificationStamp= modificationStamp;
		fNextModificationStamp= Math.max(fModificationStamp, fNextModificationStamp);
		e.fModificationStamp= fModificationStamp;

		fireDocumentChanged(e);
	}

	/**
	 * Updates all positions of all categories to the change described by the
	 * document event. All registered document updaters are called in the
	 * sequence they have been arranged. Uses a robust iterator.
	 *
	 * @param event the document event describing the change to which to adapt
	 *            the positions
	 */
	protected void updatePositions(DocumentEvent event) {
		List<IPositionUpdater> list= new ArrayList<>(fPositionUpdaters);
		Iterator<IPositionUpdater> e= list.iterator();
		while (e.hasNext()) {
			IPositionUpdater u= e.next();
			u.update(event);
		}
	}

	/**
	 * {@inheritDoc}
	 *
	 * @deprecated as of 3.0 search is provided by {@link FindReplaceDocumentAdapter}
	 */
	@Deprecated
	@Override
	public int search(int startPosition, String findString, boolean forwardSearch, boolean caseSensitive, boolean wholeWord) throws BadLocationException {
		try {
			IRegion region= getFindReplaceDocumentAdapter().find(startPosition, findString, forwardSearch, caseSensitive, wholeWord, false);
			return region == null ?  -1 : region.getOffset();
		} catch (IllegalStateException ex) {
			return -1;
		} catch (PatternSyntaxException ex) {
			return -1;
		}
	}

	/**
	 * Returns the find/replace adapter for this document.
	 *
	 * @return this document's find/replace document adapter
	 * @since 3.0
	 */
	private FindReplaceDocumentAdapter getFindReplaceDocumentAdapter() {
		if (fFindReplaceDocumentAdapter == null)
			fFindReplaceDocumentAdapter= new FindReplaceDocumentAdapter(this);

		return fFindReplaceDocumentAdapter;
	}

	/**
	 * Flushes all registered post notification changes.
	 *
	 * @since 2.0
	 */
	private void flushPostNotificationChanges() {
		if (fPostNotificationChanges != null)
			fPostNotificationChanges.clear();
	}

	/**
	 * Executes all registered post notification changes. The process is
	 * repeated until no new post notification changes are added.
	 *
	 * @since 2.0
	 */
	private void executePostNotificationChanges() {

		if (fStoppedCount > 0)
			return;

		while (fPostNotificationChanges != null) {
			List<RegisteredReplace> changes= fPostNotificationChanges;
			fPostNotificationChanges= null;

			Iterator<RegisteredReplace> e= changes.iterator();
			while (e.hasNext()) {
				RegisteredReplace replace= e.next();
				replace.fReplace.perform(this, replace.fOwner);
			}
		}
	}

	@Override
	public void acceptPostNotificationReplaces() {
		fAcceptPostNotificationReplaces= true;
	}

	@Override
	public void ignorePostNotificationReplaces() {
		fAcceptPostNotificationReplaces= false;
	}

	@Override
	public void registerPostNotificationReplace(IDocumentListener owner, IDocumentExtension.IReplace replace) {
		if (fAcceptPostNotificationReplaces) {
			if (fPostNotificationChanges == null)
				fPostNotificationChanges= new ArrayList<>(1);
			fPostNotificationChanges.add(new RegisteredReplace(owner, replace));
		}
	}

	@Override
	public void stopPostNotificationProcessing() {
		++ fStoppedCount;
	}

	@Override
	public void resumePostNotificationProcessing() {
		-- fStoppedCount;
		if (fStoppedCount == 0 && fReentranceCount == 0)
			executePostNotificationChanges();
	}

	/**
	 * {@inheritDoc}
	 *
	 * @since 2.0
	 * @deprecated since 3.1. Use
	 *             {@link IDocumentExtension4#startRewriteSession(DocumentRewriteSessionType)}
	 *             instead.
	 */
	@Deprecated
	@Override
	public void startSequentialRewrite(boolean normalized) {
	}

	/**
	 * {@inheritDoc}
	 *
	 * @since 2.0
	 * @deprecated As of 3.1, replaced by {@link IDocumentExtension4#stopRewriteSession(DocumentRewriteSession)}
	 */
	@Deprecated
	@Override
	public void stopSequentialRewrite() {
	}

	@Override
	public void resumeListenerNotification() {
		-- fStoppedListenerNotification;
		if (fStoppedListenerNotification == 0) {
			resumeDocumentListenerNotification();
		}
	}

	@Override
	public void stopListenerNotification() {
		++ fStoppedListenerNotification;
	}

	/**
	 * Resumes the document listener notification by sending out the remembered
	 * partition changed and document event.
	 *
	 * @since 2.1
	 */
	private void resumeDocumentListenerNotification() {
		if (fDeferredDocumentEvent != null) {
			DocumentEvent event= fDeferredDocumentEvent;
			fDeferredDocumentEvent= null;
			doFireDocumentChanged(event);
		}
	}

	/*
	 * @see org.eclipse.jface.text.IDocumentExtension3#computeZeroLengthPartitioning(java.lang.String, int, int)
	 * @since 3.0
	 */
	@Override
	public ITypedRegion[] computePartitioning(String partitioning, int offset, int length, boolean includeZeroLengthPartitions) throws BadLocationException, BadPartitioningException {
		if ((0 > offset) || (0 > length) || (offset + length > getLength()))
			throw new BadLocationException();

		IDocumentPartitioner partitioner= getDocumentPartitioner(partitioning);

		if (partitioner instanceof IDocumentPartitionerExtension2) {
			checkStateOfPartitioner(partitioner, partitioning);
			return ((IDocumentPartitionerExtension2) partitioner).computePartitioning(offset, length, includeZeroLengthPartitions);
		} else if (partitioner != null) {
			checkStateOfPartitioner(partitioner, partitioning);
			return partitioner.computePartitioning(offset, length);
		} else if (DEFAULT_PARTITIONING.equals(partitioning))
			return new TypedRegion[] { new TypedRegion(offset, length, DEFAULT_CONTENT_TYPE) };
		else
			throw new BadPartitioningException();
	}

	/*
	 * @see org.eclipse.jface.text.IDocumentExtension3#getZeroLengthContentType(java.lang.String, int)
	 * @since 3.0
	 */
	@Override
	public String getContentType(String partitioning, int offset, boolean preferOpenPartitions) throws BadLocationException, BadPartitioningException {
		if ((0 > offset) || (offset > getLength()))
			throw new BadLocationException();

		IDocumentPartitioner partitioner= getDocumentPartitioner(partitioning);

		if (partitioner instanceof IDocumentPartitionerExtension2) {
			checkStateOfPartitioner(partitioner, partitioning);
			return ((IDocumentPartitionerExtension2) partitioner).getContentType(offset, preferOpenPartitions);
		} else if (partitioner != null) {
			checkStateOfPartitioner(partitioner, partitioning);
			return partitioner.getContentType(offset);
		} else if (DEFAULT_PARTITIONING.equals(partitioning))
			return DEFAULT_CONTENT_TYPE;
		else
			throw new BadPartitioningException();
	}

	@Override
	public IDocumentPartitioner getDocumentPartitioner(String partitioning)  {
		return fDocumentPartitioners != null ? fDocumentPartitioners.get(partitioning) : null;
	}

	@Override
	public String[] getLegalContentTypes(String partitioning) throws BadPartitioningException {
		IDocumentPartitioner partitioner= getDocumentPartitioner(partitioning);
		if (partitioner != null)
			return partitioner.getLegalContentTypes();
		if (DEFAULT_PARTITIONING.equals(partitioning))
			return new String[] { DEFAULT_CONTENT_TYPE };
		throw new BadPartitioningException();
	}

	/*
	 * @see org.eclipse.jface.text.IDocumentExtension3#getZeroLengthPartition(java.lang.String, int)
	 * @since 3.0
	 */
	@Override
	public ITypedRegion getPartition(String partitioning, int offset, boolean preferOpenPartitions) throws BadLocationException, BadPartitioningException {
		if ((0 > offset) || (offset > getLength()))
			throw new BadLocationException();

		IDocumentPartitioner partitioner= getDocumentPartitioner(partitioning);

		if (partitioner instanceof IDocumentPartitionerExtension2) {
			checkStateOfPartitioner(partitioner, partitioning);
			return ((IDocumentPartitionerExtension2) partitioner).getPartition(offset, preferOpenPartitions);
		} else if (partitioner != null) {
			checkStateOfPartitioner(partitioner, partitioning);
			return partitioner.getPartition(offset);
		} else if (DEFAULT_PARTITIONING.equals(partitioning))
			return new TypedRegion(0, getLength(), DEFAULT_CONTENT_TYPE);
		else
			throw new BadPartitioningException();
	}

	@Override
	public String[] getPartitionings() {
		if (fDocumentPartitioners == null)
			return new String[0];
		String[] partitionings= new String[fDocumentPartitioners.size()];
		fDocumentPartitioners.keySet().toArray(partitionings);
		return partitionings;
	}

	@Override
	public void setDocumentPartitioner(String partitioning, IDocumentPartitioner partitioner) {
		if (partitioner == null) {
			if (fDocumentPartitioners != null) {
				fDocumentPartitioners.remove(partitioning);
				if (fDocumentPartitioners.size() == 0)
					fDocumentPartitioners= null;
			}
		} else {
			if (fDocumentPartitioners == null)
				fDocumentPartitioners= new HashMap<>();
			fDocumentPartitioners.put(partitioning, partitioner);
		}
		DocumentPartitioningChangedEvent event= new DocumentPartitioningChangedEvent(this);
		event.setPartitionChange(partitioning, 0, getLength());
		fireDocumentPartitioningChanged(event);
	}

	@Override
	public void repairLineInformation() {
		getTracker().set(get());
	}

	/**
	 * Fires the given event to all registered rewrite session listeners. Uses robust iterators.
	 *
	 * @param event the event to be fired
	 * @since 3.1
	 */
	protected void fireRewriteSessionChanged(DocumentRewriteSessionEvent event) {
		if (fDocumentRewriteSessionListeners.size() > 0) {
			List<IDocumentRewriteSessionListener> list= new ArrayList<>(fDocumentRewriteSessionListeners);
			Iterator<IDocumentRewriteSessionListener> e= list.iterator();
			while (e.hasNext()) {
				try {
					IDocumentRewriteSessionListener l= e.next();
					l.documentRewriteSessionChanged(event);
				} catch (Exception ex) {
					log(ex);
				}
			}
		}
	}

	@Override
	public final DocumentRewriteSession getActiveRewriteSession() {
		return fDocumentRewriteSession;
	}

	@Override
	public DocumentRewriteSession startRewriteSession(DocumentRewriteSessionType sessionType) {

		if (getActiveRewriteSession() != null)
			throw new IllegalStateException();


		fDocumentRewriteSession= new DefoldDocumentRewriteSession(sessionType);
		if (DEBUG)
			System.out.println("AbstractDocument: Starting rewrite session: " + fDocumentRewriteSession); //$NON-NLS-1$

		fireRewriteSessionChanged(new DocumentRewriteSessionEvent(this, fDocumentRewriteSession, DocumentRewriteSessionEvent.SESSION_START));

		startRewriteSessionOnPartitioners(fDocumentRewriteSession);

		ILineTracker tracker= getTracker();
		if (tracker instanceof ILineTrackerExtension) {
			ILineTrackerExtension extension= (ILineTrackerExtension) tracker;
			extension.startRewriteSession(fDocumentRewriteSession);
		}

		if (DocumentRewriteSessionType.SEQUENTIAL == sessionType)
			startSequentialRewrite(false);
		else if (DocumentRewriteSessionType.STRICTLY_SEQUENTIAL == sessionType)
			startSequentialRewrite(true);

		return fDocumentRewriteSession;
	}

	/**
	 * Starts the given rewrite session.
	 *
	 * @param session the rewrite session
	 * @since 3.1
	 */
	protected final void startRewriteSessionOnPartitioners(DocumentRewriteSession session) {
		if (fDocumentPartitioners != null) {
			Iterator<IDocumentPartitioner> e= fDocumentPartitioners.values().iterator();
			while (e.hasNext()) {
				Object partitioner= e.next();
				if (partitioner instanceof IDocumentPartitionerExtension3) {
					IDocumentPartitionerExtension3 extension= (IDocumentPartitionerExtension3) partitioner;
					extension.startRewriteSession(session);
				}
			}
		}
	}

	@Override
	public void stopRewriteSession(DocumentRewriteSession session) {
		if (fDocumentRewriteSession == session) {

			if (DEBUG)
				System.out.println("AbstractDocument: Stopping rewrite session: " + session); //$NON-NLS-1$

			DocumentRewriteSessionType sessionType= session.getSessionType();
			if (DocumentRewriteSessionType.SEQUENTIAL == sessionType || DocumentRewriteSessionType.STRICTLY_SEQUENTIAL == sessionType)
				stopSequentialRewrite();

			ILineTracker tracker= getTracker();
			if (tracker instanceof ILineTrackerExtension) {
				ILineTrackerExtension extension= (ILineTrackerExtension) tracker;
				extension.stopRewriteSession(session, get());
			}

			stopRewriteSessionOnPartitioners(fDocumentRewriteSession);

			fDocumentRewriteSession= null;
			fireRewriteSessionChanged(new DocumentRewriteSessionEvent(this, session, DocumentRewriteSessionEvent.SESSION_STOP));
		}
	}

	/**
	 * Stops the given rewrite session.
	 *
	 * @param session the rewrite session
	 * @since 3.1
	 */
	protected final void stopRewriteSessionOnPartitioners(DocumentRewriteSession session) {
		if (fDocumentPartitioners != null) {
			DocumentPartitioningChangedEvent event= new DocumentPartitioningChangedEvent(this);
			Iterator<String> e= fDocumentPartitioners.keySet().iterator();
			while (e.hasNext()) {
				String partitioning= e.next();
				IDocumentPartitioner partitioner= fDocumentPartitioners.get(partitioning);
				if (partitioner instanceof IDocumentPartitionerExtension3) {
					IDocumentPartitionerExtension3 extension= (IDocumentPartitionerExtension3) partitioner;
					extension.stopRewriteSession(session);
					event.setPartitionChange(partitioning, 0, getLength());
				}
			}
			if (!event.isEmpty())
				fireDocumentPartitioningChanged(event);
		}
	}

	@Override
	public void addDocumentRewriteSessionListener(IDocumentRewriteSessionListener listener) {
		Assert.isNotNull(listener);
		if (! fDocumentRewriteSessionListeners.contains(listener))
			fDocumentRewriteSessionListeners.add(listener);
	}

	@Override
	public void removeDocumentRewriteSessionListener(IDocumentRewriteSessionListener listener) {
		Assert.isNotNull(listener);
		fDocumentRewriteSessionListeners.remove(listener);
	}

	/**
	 * Checks the state for the given partitioner and stops the
	 * active rewrite session.
	 *
	 * @param partitioner the document partitioner to be checked
	 * @param partitioning the document partitioning the partitioner is registered for
	 * @since 3.1
	 */
	protected final void checkStateOfPartitioner(IDocumentPartitioner partitioner, String partitioning) {
		if (!(partitioner instanceof IDocumentPartitionerExtension3))
			return;

		IDocumentPartitionerExtension3 extension= (IDocumentPartitionerExtension3) partitioner;
		DocumentRewriteSession session= extension.getActiveRewriteSession();
		if (session != null) {
			extension.stopRewriteSession(session);

			if (DEBUG)
				System.out.println("AbstractDocument: Flushing rewrite session for partition type: " + partitioning); //$NON-NLS-1$

			DocumentPartitioningChangedEvent event= new DocumentPartitioningChangedEvent(this);
			event.setPartitionChange(partitioning, 0, getLength());
			fireDocumentPartitioningChanged(event);
		}
	}

	/**
	 * Returns all positions of the given category that are inside the given region.
	 *
	 * @param category the position category
	 * @param offset the start position of the region, must be >= 0
	 * @param length the length of the region, must be >= 0
	 * @param canStartBefore if <code>true</code> then positions are included
	 *            which start before the region if they end at or after the regions start
	 * @param canEndAfter if <code>true</code> then positions are included
	 *            which end after the region if they start at or before the regions end
	 * @return all positions inside the region of the given category
	 * @throws BadPositionCategoryException if category is undefined in this document
	 * @since 3.4
	 */
	public Position[] getPositions(String category, int offset, int length, boolean canStartBefore, boolean canEndAfter) throws BadPositionCategoryException {
		if (canStartBefore && canEndAfter || (!canStartBefore && !canEndAfter)) {
			List<Position> documentPositions;
			if (canStartBefore && canEndAfter) {
				if (offset < getLength() / 2) {
					documentPositions= getStartingPositions(category, 0, offset + length);
				} else {
					documentPositions= getEndingPositions(category, offset, getLength() - offset + 1);
				}
			} else {
				documentPositions= getStartingPositions(category, offset, length);
			}

			ArrayList<Position> list= new ArrayList<>(documentPositions.size());

			Position region= new Position(offset, length);

			for (Iterator<Position> iterator= documentPositions.iterator(); iterator.hasNext();) {
				Position position= iterator.next();
				if (isWithinRegion(region, position, canStartBefore, canEndAfter)) {
					list.add(position);
				}
			}

			Position[] positions= new Position[list.size()];
			list.toArray(positions);
			return positions;
		} else if (canStartBefore) {
			List<Position> list= getEndingPositions(category, offset, length);
			Position[] positions= new Position[list.size()];
			list.toArray(positions);
			return positions;
		} else {
			Assert.isLegal(canEndAfter && !canStartBefore);

			List<Position> list= getStartingPositions(category, offset, length);
			Position[] positions= new Position[list.size()];
			list.toArray(positions);
			return positions;
		}
	}

	/*
	 * @since 3.4
	 */
	private boolean isWithinRegion(Position region, Position position, boolean canStartBefore, boolean canEndAfter) {
		if (canStartBefore && canEndAfter) {
			return region.overlapsWith(position.getOffset(), position.getLength());
		} else if (canStartBefore) {
			return region.includes(position.getOffset() + position.getLength() - 1);
		} else if (canEndAfter) {
			return region.includes(position.getOffset());
		} else {
			int start= position.getOffset();
			return region.includes(start) && region.includes(start + position.getLength() - 1);
		}
	}

	/**
	 * A list of positions in the given category with an offset inside the given
	 * region. The order of the positions is arbitrary.
	 *
	 * @param category the position category
	 * @param offset the offset of the region
	 * @param length the length of the region
	 * @return a list of the positions in the region
	 * @throws BadPositionCategoryException if category is undefined in this document
	 * @since 3.4
	 */
	private List<Position> getStartingPositions(String category, int offset, int length) throws BadPositionCategoryException {
		List<Position> positions= fPositions.get(category);
		if (positions == null)
			throw new BadPositionCategoryException();

		int indexStart= computeIndexInPositionList(positions, offset, true);
		int indexEnd= computeIndexInPositionList(positions, offset + length, true);

		return positions.subList(indexStart, indexEnd);
	}

	/**
	 * A list of positions in the given category with an end position inside
	 * the given region. The order of the positions is arbitrary.
	 *
	 * @param category the position category
	 * @param offset the offset of the region
	 * @param length the length of the region
	 * @return a list of the positions in the region
	 * @throws BadPositionCategoryException if category is undefined in this document
	 * @since 3.4
	 */
	private List<Position> getEndingPositions(String category, int offset, int length) throws BadPositionCategoryException {
		List<Position> positions= fEndPositions.get(category);
		if (positions == null)
			throw new BadPositionCategoryException();

		int indexStart= computeIndexInPositionList(positions, offset, false);
		int indexEnd= computeIndexInPositionList(positions, offset + length, false);

		return positions.subList(indexStart, indexEnd);
	}

	/**
	 * Logs the given exception by reusing the code in {@link SafeRunner}.
	 *
	 * @param ex the exception
	 * @since 3.6
	 */
	private static void log(final Exception ex) {

            System.out.println(ex.getMessage());
            ex.printStackTrace();


            /**
             Do not inlcude SafeRunner since it has osgi deps - if
      		called it will invoke org/osgi/framework/BundleActivator
      		SafeRunner.run(new ISafeRunnable() {
			@Override
			public void run() throws Exception {
				throw ex;
			}

			@Override
			public void handleException(Throwable exception) {
				// NOTE: Logging is done by SafeRunner
			}
		});
            **/

	}

}
