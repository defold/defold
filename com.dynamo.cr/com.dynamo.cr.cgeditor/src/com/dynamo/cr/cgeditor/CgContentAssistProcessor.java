package com.dynamo.cr.cgeditor;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.text.BadLocationException;
import org.eclipse.jface.text.IDocument;
import org.eclipse.jface.text.ITextViewer;
import org.eclipse.jface.text.contentassist.CompletionProposal;
import org.eclipse.jface.text.contentassist.ICompletionProposal;
import org.eclipse.jface.text.contentassist.IContentAssistProcessor;
import org.eclipse.jface.text.contentassist.IContextInformation;
import org.eclipse.jface.text.contentassist.IContextInformationValidator;
import org.eclipse.ui.statushandlers.StatusManager;

public class CgContentAssistProcessor implements IContentAssistProcessor {

    static class Function {
        private String name;
        private String[] args;

        public Function(String name, String...args) {
            this.name = name;
            this.args = args;
        }
    }

    static class Namespace {
        public Namespace(Function...functions) {
            for (Function f : functions) {
                functionList.add(f);
            }
        }
        public ArrayList<Function> functionList = new ArrayList<Function>();
    }

    private HashMap<String, Namespace> namespaces;

    public CgContentAssistProcessor() {
        namespaces = new HashMap<String, Namespace>();
    }

    @Override
    public ICompletionProposal[] computeCompletionProposals(ITextViewer viewer,
            int offset) {
        List<ICompletionProposal> proposals = new ArrayList<ICompletionProposal>();

        IDocument doc = viewer.getDocument();
        try {
            int line_nr = doc.getLineOfOffset(offset);
            int line_offset = doc.getLineOffset(line_nr);

            for (String namespace_name : namespaces.keySet()) {
                String line = doc.get(line_offset, offset - line_offset);
                int ns_index = line.lastIndexOf(namespace_name + ".");
                if (ns_index != -1) {
                    String function_name = line.substring(ns_index + namespace_name.length() + 1);
                    Namespace namespace = namespaces.get(namespace_name);
                    for (Function f : namespace.functionList) {
                        if (f.name.startsWith(function_name)) {
                            String s = f.name + "(";

                            int i = 0;
                            for (String arg : f.args) {
                                s = s + arg;
                                if (i < f.args.length-1)
                                    s = s + ", ";
                                ++i;
                            }
                            s = s + ")";

                            proposals.add(new CompletionProposal(s.substring(function_name.length()), offset, 0, s.length(), null, f.name, null, null));
                        }
                    }
                }
            }

        } catch (BadLocationException e) {
            StatusManager.getManager().handle(new Status(IStatus.ERROR, CgEditorPlugin.PLUGIN_ID, e.getMessage()), StatusManager.LOG);
        }

        return proposals.toArray(new ICompletionProposal[proposals.size()]);
    }

    @Override
    public IContextInformation[] computeContextInformation(ITextViewer viewer,
            int offset) {
        return null;
    }

    @Override
    public char[] getCompletionProposalAutoActivationCharacters() {
        return new char[] {'.'};
    }

    @Override
    public char[] getContextInformationAutoActivationCharacters() {
        return null;
    }

    @Override
    public String getErrorMessage() {
        return null;
    }

    @Override
    public IContextInformationValidator getContextInformationValidator() {
        return null;
    }

}
