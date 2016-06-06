package com.defold.editor.eclipse;

import java.util.List;
import java.util.function.Function;

import javafx.geometry.Point2D;
import javafx.scene.input.KeyCode;

import org.eclipse.fx.text.ui.ITextViewer;
import org.eclipse.fx.ui.controls.styledtext.VerifyEvent;

import org.eclipse.fx.text.ui.contentassist.*;

public class DefoldContentAssistant implements IContentAssistant {
    private final Function<ContentAssistContextData, List<ICompletionProposal>> proposalComputer;
    private ITextViewer viewer;
    private DefoldContentProposalPopup popup;

    public DefoldContentAssistant(Function<ContentAssistContextData, List<ICompletionProposal>> proposalComputer) {
        this.proposalComputer = proposalComputer;
    }

    @Override
    public void install(ITextViewer textViewer) {
        if( this.viewer == null ) {
            this.viewer = textViewer;
            this.popup = new DefoldContentProposalPopup(textViewer,proposalComputer);
        }
    }

    public void doProposals(){
	List<ICompletionProposal> proposals = proposalComputer.apply(new ContentAssistContextData(this.viewer.getTextWidget().getCaretOffset(),this.viewer.getDocument()/*,""*/));

        if( proposals.size() == 1) {
            proposals.get(0).apply(this.viewer.getDocument());
            this.viewer.getTextWidget().setSelection(proposals.get(0).getSelection(this.viewer.getDocument()));
        } else if( ! proposals.isEmpty() ) {
            System.err.println();

            Point2D p = this.viewer.getTextWidget().getLocationAtOffset(this.viewer.getTextWidget().getCaretOffset());
            this.popup.displayProposals(proposals, this.viewer.getTextWidget().getCaretOffset(), this.viewer.getTextWidget().localToScreen(p));
        }
    }

}
