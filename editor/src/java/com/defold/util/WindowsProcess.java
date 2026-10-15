// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.defold.util;

import com.sun.jna.Memory;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;
import com.sun.jna.platform.win32.BaseTSD.SIZE_T;
import com.sun.jna.platform.win32.Kernel32;
import com.sun.jna.platform.win32.Win32Exception;
import com.sun.jna.platform.win32.WinBase;
import com.sun.jna.platform.win32.WinBase.PROCESS_INFORMATION;
import com.sun.jna.platform.win32.WinBase.SECURITY_ATTRIBUTES;
import com.sun.jna.platform.win32.WinBase.STARTUPINFO;
import com.sun.jna.platform.win32.WinDef.DWORD;
import com.sun.jna.platform.win32.WinNT.HANDLE;
import com.sun.jna.platform.win32.WinNT.HANDLEByReference;
import com.sun.jna.ptr.IntByReference;
import com.sun.jna.win32.StdCallLibrary;
import com.sun.jna.win32.W32APIOptions;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UncheckedIOException;
import java.lang.ProcessBuilder.Redirect;
import java.lang.ref.Cleaner;
import java.lang.ref.Reference;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.TreeMap;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** A Windows child with its own default error mode and ProcessBuilder I/O. */
public final class WindowsProcess extends Process {
    private static final Cleaner CLEANER = Cleaner.create();

    // Load Windows libraries only when this backend is used, not on import.
    private static final class Kernel {
        static final Kernel32 API = Kernel32.INSTANCE;
        @SuppressWarnings({"deprecation", "cast"}) // Also compiles against the declared JNA 4 API.
        static final ExtraAPI EXTRA = (ExtraAPI) Native.loadLibrary("kernel32", ExtraAPI.class, W32APIOptions.UNICODE_OPTIONS);
    }

    public interface ExtraAPI extends StdCallLibrary {
        HANDLE GetStdHandle(int kind);
        Pointer GetConsoleWindow();
        boolean CancelIoEx(HANDLE handle, Pointer overlapped);
        boolean ReadFile(HANDLE handle, byte[] buffer, int length, IntByReference read, Pointer overlapped);
        boolean PeekNamedPipe(HANDLE handle, Pointer buffer, int length, IntByReference read, IntByReference available, IntByReference remaining);
        boolean InitializeProcThreadAttributeList(Pointer list, int count, int flags, Pointer size);
        boolean UpdateProcThreadAttribute(Pointer list, int flags, SIZE_T attribute, Pointer value, SIZE_T size, Pointer previous, Pointer returnedSize);
        void DeleteProcThreadAttributeList(Pointer list);
        int ResumeThread(HANDLE thread);
        boolean CreateProcessW(String application, char[] command, SECURITY_ATTRIBUTES processAttributes, SECURITY_ATTRIBUTES threadAttributes,
                               boolean inheritHandles, DWORD flags, Pointer environment, String directory, StartupInfoEx startup, PROCESS_INFORMATION process);
    }

    public static final class StartupInfoEx extends Structure {
        public STARTUPINFO startup = new STARTUPINFO();
        public Pointer attributes;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("startup", "attributes");
        }
    }

    private static final class NativeMemory implements AutoCloseable {
        private static final class Allocation extends Memory {
            Allocation(long size) {
                super(size);
            }

            @SuppressWarnings("deprecation") // Memory.close() was introduced after JNA 4.
            void release() {
                dispose();
            }
        }

        final Allocation pointer;

        NativeMemory(long size) {
            pointer = new Allocation(size);
        }

        @Override
        public void close() {
            pointer.release();
        }
    }

    private static IOException error(String operation) {
        int code = Kernel.API.GetLastError();
        return new IOException(operation + ": " + new Win32Exception(code).getMessage());
    }

    private static final class NativeHandle implements Runnable, AutoCloseable {
        private final AtomicReference<HANDLE> value;

        NativeHandle(HANDLE value) {
            this.value = new AtomicReference<>(value);
        }

        HANDLE get() throws IOException {
            HANDLE handle = value.get();
            if (handle == null) throw new IOException("Stream or process handle is closed");
            return handle;
        }

        @Override
        public void run() {
            HANDLE handle = value.getAndSet(null);
            if (handle != null) {
                Kernel.EXTRA.CancelIoEx(handle, null);
                Kernel.API.CloseHandle(handle);
            }
        }

        @Override
        public void close() {
            run();
        }
    }

    private static final class PipeInput extends InputStream {
        private final NativeHandle handle;
        private final Cleaner.Cleanable cleanable;

        @SuppressWarnings("LeakingThisInConstructor") // Cleaner keeps a phantom reference; the cleanup action only holds the handle.
        PipeInput(NativeHandle handle) {
            this.handle = handle;
            cleanable = CLEANER.register(this, handle);
        }

        @Override
        public int read() throws IOException {
            byte[] value = new byte[1];
            return read(value, 0, 1) < 0 ? -1 : value[0] & 0xff;
        }

        @Override
        public int read(byte[] bytes, int offset, int length) throws IOException {
            Objects.checkFromIndexSize(offset, length, bytes.length);
            if (length == 0) return 0;
            try {
                byte[] buffer = offset == 0 ? bytes : new byte[length];
                IntByReference count = new IntByReference();
                do {
                    if (!Kernel.EXTRA.ReadFile(handle.get(), buffer, length, count, null)) {
                        int code = Kernel.API.GetLastError();
                        if (code == 109 || code == 38 || code == 232) return -1; // Broken pipe / EOF / closing pipe.
                        throw error("ReadFile");
                    }
                } while (count.getValue() == 0);
                int size = count.getValue();
                if (offset != 0) System.arraycopy(buffer, 0, bytes, offset, size);
                return size;
            } finally {
                Reference.reachabilityFence(this);
            }
        }

        @Override
        public int available() throws IOException {
            try {
                IntByReference available = new IntByReference();
                if (!Kernel.EXTRA.PeekNamedPipe(handle.get(), null, 0, null, available, null)) {
                    int code = Kernel.API.GetLastError();
                    if (code == 109 || code == 232) return 0;
                    throw error("PeekNamedPipe");
                }
                return available.getValue();
            } finally {
                Reference.reachabilityFence(this);
            }
        }

        @Override
        public void close() {
            cleanable.clean();
        }
    }

    private static final class PipeOutput extends OutputStream {
        private final NativeHandle handle;
        private final Cleaner.Cleanable cleanable;

        @SuppressWarnings("LeakingThisInConstructor") // Cleaner keeps a phantom reference; the cleanup action only holds the handle.
        PipeOutput(NativeHandle handle) {
            this.handle = handle;
            cleanable = CLEANER.register(this, handle);
        }

        @Override
        public void write(int value) throws IOException {
            write(new byte[]{(byte) value}, 0, 1);
        }

        @Override
        public void write(byte[] bytes, int offset, int length) throws IOException {
            Objects.checkFromIndexSize(offset, length, bytes.length);
            try {
                while (length > 0) {
                    byte[] buffer = offset == 0 ? bytes : Arrays.copyOfRange(bytes, offset, offset + length);
                    IntByReference written = new IntByReference();
                    if (!Kernel.API.WriteFile(handle.get(), buffer, length, written, null)) throw error("WriteFile");
                    int count = written.getValue();
                    if (count == 0) throw new IOException("WriteFile made no progress");
                    offset += count;
                    length -= count;
                }
            } finally {
                Reference.reachabilityFence(this);
            }
        }

        @Override
        public void close() {
            cleanable.clean();
        }
    }

    private static final class Redirection implements AutoCloseable {
        final NativeHandle child;
        NativeHandle parent;

        Redirection(Redirect redirect, int stream) throws IOException {
            SECURITY_ATTRIBUTES attributes = new SECURITY_ATTRIBUTES();
            attributes.bInheritHandle = true;
            switch (redirect.type()) {
                case PIPE -> {
                    HANDLEByReference read = new HANDLEByReference();
                    HANDLEByReference write = new HANDLEByReference();
                    if (!Kernel.API.CreatePipe(read, write, attributes, 0)) throw error("CreatePipe");
                    child = new NativeHandle(stream == 0 ? read.getValue() : write.getValue());
                    parent = new NativeHandle(stream == 0 ? write.getValue() : read.getValue());
                    if (!Kernel.API.SetHandleInformation(parent.get(), 1, 0)) {
                        IOException failure = error("SetHandleInformation");
                        close();
                        throw failure;
                    }
                }
                case INHERIT -> {
                    HANDLE source = Kernel.EXTRA.GetStdHandle(-10 - stream);
                    if (source == null || WinBase.INVALID_HANDLE_VALUE.equals(source)) {
                        child = openFile("NUL", stream == 0, false, attributes);
                    } else {
                        HANDLEByReference copy = new HANDLEByReference();
                        HANDLE self = Kernel.API.GetCurrentProcess();
                        if (!Kernel.API.DuplicateHandle(self, source, self, copy, 0, true, 2)) throw error("DuplicateHandle");
                        child = new NativeHandle(copy.getValue());
                    }
                }
                default -> child = openFile(redirect.file().getPath(), stream == 0, redirect.type() == Redirect.Type.APPEND, attributes);
            }
        }

        private static NativeHandle openFile(String path, boolean input, boolean append, SECURITY_ATTRIBUTES attributes) throws IOException {
            if (path.indexOf('\0') >= 0) throw new IOException("NUL in redirection path");
            HANDLE handle = Kernel.API.CreateFile(path, input ? 0x80000000 : append ? 0x4 : 0x40000000,
                                                  0x7, attributes, input ? 3 : append ? 4 : 2, 0x80, null);
            if (WinBase.INVALID_HANDLE_VALUE.equals(handle)) throw error("CreateFile " + path);
            return new NativeHandle(handle);
        }

        InputStream input() {
            if (parent == null) return InputStream.nullInputStream();
            InputStream stream = new PipeInput(parent);
            parent = null;
            return stream;
        }

        OutputStream output() {
            if (parent == null) {
                return new OutputStream() {
                    @Override
                    public void write(int value) throws IOException {
                        throw new IOException("Process stdin is redirected");
                    }

                    @Override
                    public void write(byte[] bytes, int offset, int length) throws IOException {
                        Objects.checkFromIndexSize(offset, length, bytes.length);
                        throw new IOException("Process stdin is redirected");
                    }
                };
            }
            OutputStream stream = new PipeOutput(parent);
            parent = null;
            return stream;
        }

        @Override
        public void close() {
            child.close();
            if (parent != null) parent.close();
        }
    }

    private static final class State {
        final NativeHandle nativeHandle;
        final ProcessHandle handle;
        final CompletableFuture<Integer> exited = new CompletableFuture<>();
        Integer exitCode;

        State(NativeHandle nativeHandle, long pid) throws IOException {
            this.nativeHandle = nativeHandle;
            // Creation is suspended until this handle and the streams exist.
            handle = ProcessHandle.of(pid).orElseThrow(() -> new IOException("New process is unavailable: " + pid));
            handle.onExit().whenComplete((ignored, failure) -> {
                int code;
                try (nativeHandle) {
                    if (failure != null) throw new IllegalStateException("Process exit notification failed", failure);
                    code = exitValue();
                } catch (IOException | RuntimeException | Error error) {
                    exited.completeExceptionally(error);
                    return;
                }
                exited.complete(code);
            });
        }

        synchronized int exitValue() throws IOException {
            if (exitCode != null) return exitCode;
            int status = Kernel.API.WaitForSingleObject(nativeHandle.get(), 0);
            if (status == 258) throw new IllegalThreadStateException("Process has not exited");
            if (status != 0) throw error("WaitForSingleObject");
            IntByReference code = new IntByReference();
            if (!Kernel.API.GetExitCodeProcess(nativeHandle.get(), code)) throw error("GetExitCodeProcess");
            exitCode = code.getValue();
            return exitCode;
        }

        synchronized void destroy() {
            if (handle.isAlive()) {
                try {
                    if (!Kernel.API.TerminateProcess(nativeHandle.get(), 1) && handle.isAlive()) {
                        throw new UncheckedIOException(error("TerminateProcess"));
                    }
                } catch (IOException exception) {
                    throw new UncheckedIOException(exception);
                }
            }
        }
    }

    private final State state;
    private final OutputStream stdin;
    private final InputStream stdout;
    private final InputStream stderr;

    private WindowsProcess(State state, OutputStream stdin, InputStream stdout, InputStream stderr) {
        this.state = state;
        this.stdin = stdin;
        this.stdout = stdout;
        this.stderr = stderr;
    }

    private static String quote(String argument) {
        StringBuilder result = new StringBuilder("\"");
        int backslashes = 0;
        for (int i = 0; i < argument.length(); ++i) {
            char c = argument.charAt(i);
            if (c == '\\') {
                ++backslashes;
            } else {
                result.append("\\".repeat(c == '"' ? backslashes * 2 + 1 : backslashes));
                result.append(c);
                backslashes = 0;
            }
        }
        return result.append("\\".repeat(backslashes * 2)).append('"').toString();
    }

    private static char[] commandLine(List<String> command) throws IOException {
        if (command.isEmpty()) throw new IndexOutOfBoundsException("Empty command");
        for (String argument : command) {
            Objects.requireNonNull(argument);
            if (argument.indexOf('\0') >= 0) throw new IOException("NUL in process argument");
        }
        String executable = command.get(0);
        if (executable.isEmpty() || executable.indexOf('"') >= 0) throw new IOException("Invalid executable path: " + executable);
        boolean batch = executable.toLowerCase(Locale.ROOT).endsWith(".cmd") || executable.toLowerCase(Locale.ROOT).endsWith(".bat");
        List<String> arguments = new ArrayList<>();
        for (String argument : command) {
            if (batch && argument.indexOf('"') >= 0) throw new IOException("Quotes in batch arguments require an explicit cmd.exe command");
            arguments.add(batch ? '"' + argument + '"' : quote(argument));
        }
        String line = String.join(" ", arguments);
        if (batch) {
            String shell = System.getenv("ComSpec");
            if (shell == null) shell = new File(System.getenv("SystemRoot"), "System32/cmd.exe").getPath();
            line = quote(shell) + " /d /s /c \"" + line + '"';
        }
        return (line + '\0').toCharArray();
    }

    public static Process start(ProcessBuilder builder) throws IOException {
        char[] command = commandLine(builder.command());
        TreeMap<String, String> environment = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        environment.putAll(builder.environment());
        String systemRoot = System.getenv("SystemRoot");
        if (systemRoot != null) environment.putIfAbsent("SystemRoot", systemRoot);
        if (builder.directory() != null && builder.directory().getPath().indexOf('\0') >= 0) throw new IOException("NUL in process directory");
        StringBuilder environmentBlock = new StringBuilder();
        for (Map.Entry<String, String> entry : environment.entrySet()) {
            environmentBlock.append(entry.getKey()).append('=').append(entry.getValue()).append('\0');
        }
        if (environment.isEmpty()) environmentBlock.append('\0');
        byte[] environmentBytes = environmentBlock.append('\0').toString().getBytes(StandardCharsets.UTF_16LE);
        try {
            // OpenJDK serializes native Windows creation on this class. Use its
            // lock while child pipe handles are inheritable, including during
            // concurrent launches that still use ProcessBuilder.
            synchronized (Class.forName("java.lang.ProcessImpl")) {
                return start(builder, command, environmentBytes);
            }
        } catch (ClassNotFoundException exception) {
            throw new IOException("Windows process implementation is unavailable", exception);
        }
    }

    private static Process start(ProcessBuilder builder, char[] command, byte[] environmentBytes) throws IOException {
        try (Redirection input = new Redirection(builder.redirectInput(), 0);
             Redirection output = new Redirection(builder.redirectOutput(), 1);
             Redirection error = builder.redirectErrorStream() ? null : new Redirection(builder.redirectError(), 2);
             NativeMemory environment = new NativeMemory(environmentBytes.length);
             NativeMemory size = new NativeMemory(Native.POINTER_SIZE)) {
            environment.pointer.write(0, environmentBytes, 0, environmentBytes.length);
            size.pointer.clear();
            Kernel.EXTRA.InitializeProcThreadAttributeList(null, 1, 0, size.pointer);
            long attributeSize = Native.POINTER_SIZE == 8 ? size.pointer.getLong(0) : Integer.toUnsignedLong(size.pointer.getInt(0));
            if (attributeSize == 0) throw error("InitializeProcThreadAttributeList size");
            LinkedHashSet<HANDLE> inherited = new LinkedHashSet<>(Arrays.asList(input.child.get(), output.child.get(), error == null ? output.child.get() : error.child.get()));
            try (NativeMemory attributes = new NativeMemory(attributeSize);
                 NativeMemory handles = new NativeMemory((long) inherited.size() * Native.POINTER_SIZE)) {
                int index = 0;
                for (HANDLE handle : inherited) handles.pointer.setPointer((long) index++ * Native.POINTER_SIZE, handle.getPointer());
                if (!Kernel.EXTRA.InitializeProcThreadAttributeList(attributes.pointer, 1, 0, size.pointer)) throw error("InitializeProcThreadAttributeList");
                try {
                    if (!Kernel.EXTRA.UpdateProcThreadAttribute(attributes.pointer, 0, new SIZE_T(0x20002), handles.pointer, new SIZE_T(handles.pointer.size()), null, null)) {
                        throw error("UpdateProcThreadAttribute HANDLE_LIST");
                    }
                    StartupInfoEx startup = new StartupInfoEx();
                    startup.startup.cb = new DWORD(startup.size());
                    startup.startup.dwFlags = WinBase.STARTF_USESTDHANDLES;
                    startup.startup.hStdInput = input.child.get();
                    startup.startup.hStdOutput = output.child.get();
                    startup.startup.hStdError = error == null ? output.child.get() : error.child.get();
                    startup.attributes = attributes.pointer;
                    int flags = WinBase.CREATE_DEFAULT_ERROR_MODE | WinBase.CREATE_UNICODE_ENVIRONMENT | WinBase.CREATE_SUSPENDED | 0x80000;
                    boolean inheritConsole = builder.redirectInput() == Redirect.INHERIT || builder.redirectOutput() == Redirect.INHERIT
                                             || (!builder.redirectErrorStream() && builder.redirectError() == Redirect.INHERIT);
                    if (!inheritConsole || Kernel.EXTRA.GetConsoleWindow() == null) flags |= WinBase.CREATE_NO_WINDOW;
                    PROCESS_INFORMATION info = new PROCESS_INFORMATION();
                    if (!Kernel.EXTRA.CreateProcessW(
                            /* application */ null,
                            /* command */ command,
                            /* processAttributes */ null,
                            /* threadAttributes */ null,
                            /* inheritHandles */ true,
                            /* flags */ new DWORD(flags),
                            /* environment */ environment.pointer,
                            /* directory */ builder.directory() == null ? null : builder.directory().getAbsolutePath(),
                            /* startup */ startup,
                            /* process */ info)) {
                        throw error("CreateProcessW");
                    }
                    NativeHandle process = new NativeHandle(info.hProcess);
                    OutputStream stdin = input.output();
                    InputStream stdout = output.input();
                    InputStream stderr = error == null ? InputStream.nullInputStream() : error.input();
                    boolean started = false;
                    try {
                        WindowsProcess result = new WindowsProcess(new State(process, Integer.toUnsignedLong(info.dwProcessId.intValue())), stdin, stdout, stderr);
                        if (Kernel.EXTRA.ResumeThread(info.hThread) == -1) throw error("ResumeThread");
                        started = true;
                        return result;
                    } finally {
                        Kernel.API.CloseHandle(info.hThread);
                        if (!started) {
                            Kernel.API.TerminateProcess(info.hProcess, 1);
                            process.close();
                            stdin.close();
                            stdout.close();
                            stderr.close();
                        }
                    }
                } finally {
                    Kernel.EXTRA.DeleteProcThreadAttributeList(attributes.pointer);
                }
            }
        }
    }

    @Override public OutputStream getOutputStream() { return stdin; }
    @Override public InputStream getInputStream() { return stdout; }
    @Override public InputStream getErrorStream() { return stderr; }
    @Override public boolean isAlive() { return state.handle.isAlive(); }
    @Override public long pid() { return state.handle.pid(); }
    @Override public ProcessHandle toHandle() { return state.handle; }
    @Override public boolean supportsNormalTermination() { return false; }
    @Override public void destroy() { state.destroy(); }
    @Override public Process destroyForcibly() { destroy(); return this; }
    @Override public CompletableFuture<Process> onExit() { return state.exited.thenApply(ignored -> this); }

    @Override
    public int exitValue() {
        try {
            return state.exitValue();
        } catch (IOException exception) {
            throw new UncheckedIOException(exception);
        }
    }

    @Override
    public int waitFor() throws InterruptedException {
        try {
            return state.exited.get();
        } catch (ExecutionException exception) {
            throw new IllegalStateException("Waiting for process failed", exception.getCause());
        }
    }

    @Override
    public boolean waitFor(long timeout, TimeUnit unit) throws InterruptedException {
        try {
            state.exited.get(timeout, unit);
            return true;
        } catch (TimeoutException exception) {
            return false;
        } catch (ExecutionException exception) {
            throw new IllegalStateException("Waiting for process failed", exception.getCause());
        }
    }
}
