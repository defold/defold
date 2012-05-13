package com.dynamo.cr.cgeditor;

import org.eclipse.jface.text.IDocument;
import org.eclipse.jface.text.ITextDoubleClickStrategy;
import org.eclipse.jface.text.TextAttribute;
import org.eclipse.jface.text.contentassist.ContentAssistant;
import org.eclipse.jface.text.contentassist.IContentAssistant;
import org.eclipse.jface.text.presentation.IPresentationReconciler;
import org.eclipse.jface.text.presentation.PresentationReconciler;
import org.eclipse.jface.text.rules.DefaultDamagerRepairer;
import org.eclipse.jface.text.rules.Token;
import org.eclipse.jface.text.source.ISourceViewer;
import org.eclipse.jface.text.source.SourceViewerConfiguration;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Display;

public class CgConfiguration extends SourceViewerConfiguration {
	private CgDoubleClickStrategy doubleClickStrategy;
	private CgScanner tagScanner;
	private ColorManager colorManager;

	public CgConfiguration(ColorManager colorManager) {
		this.colorManager = colorManager;
	}
	public String[] getConfiguredContentTypes(ISourceViewer sourceViewer) {
		return new String[] {
			IDocument.DEFAULT_CONTENT_TYPE,
			CgPartitionScanner.CG_COMMENT };
	}
	public ITextDoubleClickStrategy getDoubleClickStrategy(
		ISourceViewer sourceViewer,
		String contentType) {
		if (doubleClickStrategy == null)
			doubleClickStrategy = new CgDoubleClickStrategy();
		return doubleClickStrategy;
	}

	protected CgScanner getCgScanner() {
		if (tagScanner == null) {
			tagScanner = new CgScanner(colorManager);
			tagScanner.setDefaultReturnToken(
				new Token(
					new TextAttribute(
						colorManager.getColor(ICgColorConstants.DEFAULT))));
		}
		return tagScanner;
	}

	public IPresentationReconciler getPresentationReconciler(ISourceViewer sourceViewer) {
		PresentationReconciler reconciler = new PresentationReconciler();

	      DefaultDamagerRepairer dr =
	            new DefaultDamagerRepairer(getCgScanner());
	        reconciler.setDamager(dr, IDocument.DEFAULT_CONTENT_TYPE);
	        reconciler.setRepairer(dr, IDocument.DEFAULT_CONTENT_TYPE);

		NonRuleBasedDamagerRepairer ndr =
			new NonRuleBasedDamagerRepairer(
				new TextAttribute(
					colorManager.getColor(ICgColorConstants.COMMENT)));
		reconciler.setDamager(ndr, CgPartitionScanner.CG_COMMENT);
		reconciler.setRepairer(ndr, CgPartitionScanner.CG_COMMENT);

		return reconciler;
	}

	@Override
	public IContentAssistant getContentAssistant(ISourceViewer sourceViewer) {
	    ContentAssistant assistant= new ContentAssistant();

	    assistant.setDocumentPartitioning(getConfiguredDocumentPartitioning(sourceViewer));
	    assistant.setContentAssistProcessor(new CgContentAssistProcessor(), IDocument.DEFAULT_CONTENT_TYPE);

	    assistant.setAutoActivationDelay(500);
	    assistant.enableAutoActivation(true);

	    assistant.setProposalSelectorBackground(Display.getDefault().
	    getSystemColor(SWT.COLOR_WHITE));

	    return assistant;
	}

}