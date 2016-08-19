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
import com.dynamo.scriptdoc.proto.ScriptDoc.ReturnValue;
import com.dynamo.scriptdoc.proto.ScriptDoc.Type;

public class LuaContentAssistProcessor implements IContentAssistProcessor {

    public LuaContentAssistProcessor() {
    }

    public static LuaParseResult parseLine(String line) {
        Pattern pattern1 = Pattern.compile("([a-zA-Z0-9_]+)([\\(]?)");
        Pattern pattern2 = Pattern.compile("([a-zA-Z0-9_]+)\\.([a-zA-Z0-9_]*)([\\(]?)");

        Matcher matcher1 = pattern1.matcher(line);
        Matcher matcher2 = pattern2.matcher(line);
        LuaParseResult result = null;

        while (matcher2.find()) {
            int start = matcher2.start();
            int end = matcher2.end();
            boolean inFunction = matcher2.group(3).length() > 0;
            if (inFunction)
                --end;
            result = new LuaParseResult(matcher2.group(1), matcher2.group(2), inFunction, start, end);
        }

        if (result != null) {
            return result;
        }

        while (matcher1.find()) {
            int start = matcher1.start();
            int end = matcher1.end();
            boolean inFunction = matcher1.group(2).length() > 0;
            if (inFunction)
                --end;
            result = new LuaParseResult("", matcher1.group(1), inFunction, start, end);
        }

        if (result == null) {
            // If no replacement if found return "empty" result
            return new LuaParseResult("", "", false, 0, 0);
        }
        else {
            return result;
        }
    }

    @Override
    public ICompletionProposal[] computeCompletionProposals(ITextViewer viewer,
            int offset) {
        List<ICompletionProposal> proposals = new ArrayList<ICompletionProposal>();

        Image packageImage = LuaEditorPlugin.getDefault().getImage(LuaEditorPlugin.PACKAGE_IMAGE_ID);
        Image functionImage = LuaEditorPlugin.getDefault().getImage(LuaEditorPlugin.FUNCTION_IMAGE_ID);
        Image constantImage = LuaEditorPlugin.getDefault().getImage(LuaEditorPlugin.CONSTANT_IMAGE_ID);
        Image messageImage = LuaEditorPlugin.getDefault().getImage(LuaEditorPlugin.MESSAGE_IMAGE_ID);

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

                    additionalInfo.append("<br>");
                    additionalInfo.append("<b>Returns:</b>");
                    additionalInfo.append("<br>");
                    
                    for (ReturnValue returnvalue : element.getReturnvaluesList()) {
                        additionalInfo.append("&#160;&#160;&#160;&#160;<b>");
                        additionalInfo.append(returnvalue.getName());
                        additionalInfo.append("</b> ");
                        additionalInfo.append(returnvalue.getDoc());
                        additionalInfo.append("<br>");
                    }
                } else if (element.getType() == Type.NAMESPACE) {
                    // We do nothing currently
                }

                int completeLength = parseResult.getNamespace().length() + parseResult.getFunction().length() + 1;

                Image image = null;
                switch (element.getType()) {
                case FUNCTION:
                    image = functionImage;
                    break;

                case MESSAGE:
                    image = messageImage;
                    break;

                case NAMESPACE:
                    image = packageImage;
                    break;

                case VARIABLE:
                    image = constantImage;
                    break;
                }

                int cursorPosition = s.length() - completeLength;
                if (parseResult.inFunction()) {
                    // If we are in a function, eg gui.animate(... only show the list of items and do not insert
                    proposals.add(new CompletionProposal("", offset, 0, 0, image, s, null, additionalInfo.toString()));
                } else {
                    // Default case, do actual completion
                    if (parseResult.getNamespace().equals("")) {
                        /*
                         * Special case for replacing stuff in the global namespace ("")
                         * This was required due to that the replacement code didn't work for completation with ""
                         * Some off-by-one error. We should really have unit-test for replacements..
                         */
                        int matchLen = parseResult.getMatchEnd() - parseResult.getMatchStart();
                        int replacementOffset = offset;
                        int replacementLength = 0;
                        String replacementString = s.substring(matchLen);
                        int newCursorPosition = cursorPosition + 1;
                        proposals.add(new CompletionProposal(replacementString, replacementOffset, replacementLength, newCursorPosition, image, s, null, additionalInfo.toString()));

                    } else {
                        /*
                         * General case
                         */
                        int matchLen = parseResult.getMatchEnd() - parseResult.getMatchStart();
                        int replacementOffset = offset - matchLen + parseResult.getNamespace().length() + 1;
                        int replacementLength = matchLen - parseResult.getNamespace().length() - 1;
                        String replacementString = s.substring(parseResult.getNamespace().length() + 1);
                        int newCursorPosition = cursorPosition + parseResult.getFunction().length();
                        proposals.add(new CompletionProposal(replacementString, replacementOffset, replacementLength, newCursorPosition, image, s, null, additionalInfo.toString()));
                    }
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
