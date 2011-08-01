package com.dynamo.cr.luaeditor;

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

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
import org.eclipse.swt.graphics.Image;
import org.eclipse.ui.statushandlers.StatusManager;

import com.dynamo.scriptdoc.proto.ScriptDoc;
import com.dynamo.scriptdoc.proto.ScriptDoc.Parameter;
import com.dynamo.scriptdoc.proto.ScriptDoc.Type;

public class LuaContentAssistProcessor implements IContentAssistProcessor {

    public LuaContentAssistProcessor() {
    }

    public static LuaParseResult parseLine(String line) {
        Pattern pattern = Pattern.compile("([a-zA-Z0-9_]+)\\.([a-zA-Z0-9_]*)([\\(]?)");
        Matcher matcher = pattern.matcher(line);
        LuaParseResult result = null;

        while (matcher.find()) {
            int start = matcher.start();
            int end = matcher.end();
            boolean inFunction = matcher.group(3).length() > 0;
            if (inFunction)
                --end;
            result = new LuaParseResult(matcher.group(1), matcher.group(2), inFunction, start, end);
        }
        return result;
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

            LuaParseResult parseResult = parseLine(line);
            if (parseResult == null)
                return new ICompletionProposal[0];

            ScriptDoc.Element[] elements = LuaEditorPlugin.getDefault().getDocumentation(parseResult);
            for (ScriptDoc.Element element : elements) {
                StringBuffer additionalInfo = new StringBuffer();
                additionalInfo.append(element.getDescription());
                String s = element.getName();
                if (element.getType() == Type.FUNCTION) {
                    s += "(";
                    additionalInfo.append("<br><br>");
                    additionalInfo.append("<b>Parameters:</b>");
                    additionalInfo.append("<br>");

                    int i = 0;
                    for (Parameter parameter : element.getParametersList()) {
                        additionalInfo.append("&#160;&#160;&#160;&#160;<b>");
                        additionalInfo.append(parameter.getName());
                        additionalInfo.append("</b> ");
                        additionalInfo.append(parameter.getDoc());
                        additionalInfo.append("<br>");

                        s = s + parameter.getName();
                        if (i < element.getParametersCount()-1)
                            s = s + ", ";
                        ++i;
                    }
                    s = s + ")";

                    if (element.getReturn().length() > 0) {
                        additionalInfo.append("<br>");
                        additionalInfo.append("<b>Returns:</b><br>");
                        additionalInfo.append("&#160;&#160;&#160;&#160;<b>");
                        additionalInfo.append(element.getReturn());
                    }
                }

                int completeLength = parseResult.getNamespace().length() + parseResult.getFunction().length() + 1;

                int cursorPosition = s.length() - completeLength;
                if (parseResult.inFunction()) {
                    // If we are in a function, eg gui.animate(... only show the list of items and do not insert
                    proposals.add(new CompletionProposal("", offset, 0, 0, luaImage, s, null, additionalInfo.toString()));
                } else {
                    // Default case, do actual completion
                    int matchLen = parseResult.getMatchEnd() - parseResult.getMatchStart();
                    int replacementOffset = offset - matchLen + parseResult.getNamespace().length() + 1;
                    int replacementLength = matchLen - parseResult.getNamespace().length() - 1;
                    String replacementString = s.substring(parseResult.getNamespace().length() + 1);
                    int newCursorPosition = cursorPosition + parseResult.getFunction().length();
                    proposals.add(new CompletionProposal(replacementString, replacementOffset, replacementLength, newCursorPosition, luaImage, s, null, additionalInfo.toString()));
                }

            }
        } catch (BadLocationException e) {
            Status status = new Status(IStatus.ERROR, LuaEditorPlugin.PLUGIN_ID, e.getMessage(), e);
            StatusManager.getManager().handle(status, StatusManager.LOG);
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
