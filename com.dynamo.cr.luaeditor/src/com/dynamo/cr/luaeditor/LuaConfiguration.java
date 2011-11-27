package com.dynamo.cr.luaeditor;

import org.eclipse.jface.internal.text.html.HTMLTextPresenter;
import org.eclipse.jface.text.DefaultInformationControl;
import org.eclipse.jface.text.IDocument;
import org.eclipse.jface.text.IInformationControl;
import org.eclipse.jface.text.IInformationControlCreator;
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
import org.eclipse.swt.widgets.Shell;

@SuppressWarnings("restriction")
public class LuaConfiguration extends SourceViewerConfiguration {
    private LuaDoubleClickStrategy doubleClickStrategy;
    private LuaScanner tagScanner;
    private ColorManager colorManager;

    public LuaConfiguration(ColorManager colorManager) {
        this.colorManager = colorManager;
    }
    @Override
    public String[] getConfiguredContentTypes(ISourceViewer sourceViewer) {
        return new String[] {
                IDocument.DEFAULT_CONTENT_TYPE,
                LuaPartitionScanner.LUA_COMMENT_SINGLE,
                LuaPartitionScanner.LUA_COMMENT_MULTI };
    }
    @Override
    public ITextDoubleClickStrategy getDoubleClickStrategy(
            ISourceViewer sourceViewer,
            String contentType) {
        if (doubleClickStrategy == null)
            doubleClickStrategy = new LuaDoubleClickStrategy();
        return doubleClickStrategy;
    }

    protected LuaScanner getLuaScanner() {
        if (tagScanner == null) {
            tagScanner = new LuaScanner(colorManager);
            tagScanner.setDefaultReturnToken(
                    new Token(
                            new TextAttribute(
                                    colorManager.getColor(ILuaColorConstants.DEFAULT))));
        }
        return tagScanner;
    }

    @Override
    public IPresentationReconciler getPresentationReconciler(ISourceViewer sourceViewer) {
        PresentationReconciler reconciler = new PresentationReconciler();
        reconciler.setDocumentPartitioning(getConfiguredDocumentPartitioning(sourceViewer));

        DefaultDamagerRepairer dr;

        dr = new DefaultDamagerRepairer(new LuaSingleCommentScanner(colorManager));
        reconciler.setDamager(dr, LuaPartitionScanner.LUA_COMMENT_SINGLE);
        reconciler.setRepairer(dr, LuaPartitionScanner.LUA_COMMENT_SINGLE);

        dr = new DefaultDamagerRepairer(new LuaMultiCommentScanner(colorManager));
        reconciler.setDamager(dr, LuaPartitionScanner.LUA_COMMENT_MULTI);
        reconciler.setRepairer(dr, LuaPartitionScanner.LUA_COMMENT_MULTI);

        dr = new DefaultDamagerRepairer(getLuaScanner());
        reconciler.setDamager(dr, IDocument.DEFAULT_CONTENT_TYPE);
        reconciler.setRepairer(dr, IDocument.DEFAULT_CONTENT_TYPE);

        return reconciler;
    }

    @Override
    public String getConfiguredDocumentPartitioning(ISourceViewer sourceViewer) {
        return "com.dynamo.cr.luaeditor.partitioning";
    }

    @Override
    public IContentAssistant getContentAssistant(ISourceViewer sourceViewer) {
        ContentAssistant assistant= new ContentAssistant();

        assistant.setDocumentPartitioning(getConfiguredDocumentPartitioning(sourceViewer));
        assistant.setContentAssistProcessor(new LuaContentAssistProcessor(), IDocument.DEFAULT_CONTENT_TYPE);
        assistant.setShowEmptyList(true);
        assistant.setAutoActivationDelay(500);
        assistant.enableAutoActivation(true);

        assistant.setInformationControlCreator(new IInformationControlCreator() {
            @Override
            public IInformationControl createInformationControl(Shell parent) {
                return new DefaultInformationControl(parent, new HTMLTextPresenter());
            }
        });

        return assistant;
    }

}