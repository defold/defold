package com.dynamo.cr.luaeditor;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import org.eclipse.jface.text.BadLocationException;
import org.eclipse.jface.text.IDocument;
import org.eclipse.jface.text.ITextViewer;
import org.eclipse.jface.text.contentassist.CompletionProposal;
import org.eclipse.jface.text.contentassist.ICompletionProposal;
import org.eclipse.jface.text.contentassist.IContentAssistProcessor;
import org.eclipse.jface.text.contentassist.IContextInformation;
import org.eclipse.jface.text.contentassist.IContextInformationValidator;

public class LuaContentAssistProcessor implements IContentAssistProcessor {

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

    public LuaContentAssistProcessor() {
        namespaces = new HashMap<String, Namespace>();
        namespaces.put("go", new Namespace(new Function("get_position", "self"),
                                           new Function("post", "\"message_name\""),
                                           new Function("post_to", "id", "\"component\"", "\"message_name\""),
                                           new Function("get_position", "self"),
                                           new Function("get_rotation", "self"),
                                           new Function("set_position", "self", "position"),
                                           new Function("set_rotation", "self", "rotation"),
                                           new Function("get_world_position", "self"),
                                           new Function("get_id", "self", "\"goname\""),
                                           new Function("is_visible", "min, max, margin"),
                                           new Function("delete", "self")
                                           ));
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
            e.printStackTrace();
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
