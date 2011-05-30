package com.dynamo.cr.luaeditor;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.jface.text.BadLocationException;
import org.eclipse.jface.text.IDocument;
import org.eclipse.jface.text.ITextViewer;
import org.eclipse.jface.text.contentassist.CompletionProposal;
import org.eclipse.jface.text.contentassist.ICompletionProposal;
import org.eclipse.jface.text.contentassist.IContentAssistProcessor;
import org.eclipse.jface.text.contentassist.IContextInformation;
import org.eclipse.jface.text.contentassist.IContextInformationValidator;
import org.eclipse.swt.graphics.Image;

import com.dynamo.scriptdoc.proto.ScriptDoc;
import com.dynamo.scriptdoc.proto.ScriptDoc.Parameter;

public class LuaContentAssistProcessor implements IContentAssistProcessor {

    public LuaContentAssistProcessor() {
    }

    @Override
    public ICompletionProposal[] computeCompletionProposals(ITextViewer viewer,
            int offset) {
        List<ICompletionProposal> proposals = new ArrayList<ICompletionProposal>();

        Image luaImage = LuaEditorPlugin.getDefault().getLuaImage();
        IDocument doc = viewer.getDocument();
        try {
            int line_nr = doc.getLineOfOffset(offset);
            int line_offset = doc.getLineOffset(line_nr);

            String line = doc.get(line_offset, offset - line_offset);
            line = line.replaceAll("^\\s+", "");

            int index = line.length() - 2;
            while (index >= 1 && (Character.isLetter(line.charAt(index-1)) || line.charAt(index-1) == '.')) {
                --index;
            }
            line = line.substring(index);

            ScriptDoc.Function[] functions = LuaEditorPlugin.getDefault().getDocumentation(line);
            for (ScriptDoc.Function function : functions) {
                StringBuffer additionalInfo = new StringBuffer();
                additionalInfo.append(function.getDescription());
                additionalInfo.append("<br><br>");
                additionalInfo.append("<b>Parameters:</b>");
                additionalInfo.append("<br>");

                String s = function.getName() + "(";
                int i = 0;
                for (Parameter parameter : function.getParametersList()) {
                    additionalInfo.append("&#160;&#160;&#160;&#160;<b>");
                    additionalInfo.append(parameter.getName());
                    additionalInfo.append("</b> ");
                    additionalInfo.append(parameter.getDoc());
                    additionalInfo.append("<br>");

                    s = s + parameter.getName();
                    if (i < function.getParametersCount()-1)
                        s = s + ", ";
                    ++i;
                }
                s = s + ")";

                if (function.getReturn().length() > 0) {
                    additionalInfo.append("<br>");
                    additionalInfo.append("<b>Returns:</b><br>");
                    additionalInfo.append("&#160;&#160;&#160;&#160;<b>");
                    additionalInfo.append(function.getReturn());
                }

                int cursorPosition = s.length() - line.length();
                proposals.add(new CompletionProposal(s.substring(line.length()), offset, 0, cursorPosition, luaImage, s, null, additionalInfo.toString()));
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
