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

package com.dynamo.bob;

import com.dynamo.bob.bundle.ICanceled;

import java.math.BigDecimal;
import java.math.BigInteger;
import java.math.MathContext;
import java.util.Objects;
import java.util.function.UnaryOperator;
import java.util.concurrent.atomic.AtomicReference;

public final class Progress implements IProgress {
    public interface Reporter extends ICanceled, AutoCloseable {
        void report(Message message, double fraction);

        @Override
        default void close() {
        }

        @Override
        default boolean isCanceled() {
            return false;
        }
    }

    /// Progress whose messages are held back until {@link #releaseMessages()} is called.
    ///
    /// Work is forwarded immediately. Once released, the latest held message is
    /// emitted and later messages pass through until the progress is closed.
    public interface Deferred extends IProgress {
        void releaseMessages();
    }

    private final Reporter reporter;
    private final Object reportLock = new Object();
    private ProgressState lastReportedState;
    private boolean reporterClosed;
    private final AtomicReference<ProgressState> state = new AtomicReference<>(new ProgressState(Rational.ZERO, Message.Working.INSTANCE, false));

    public Progress(Reporter reporter) {
        this.reporter = Objects.requireNonNull(reporter);
    }

    public static Progress discarding() {
        return new Progress((_, _) -> {
        });
    }

    /// Returns a subtask spanning all of `progress` whose messages are deferred
    public static Deferred deferMessages(IProgress progress) {
        return new SubProgress(progress, totalCapacity(progress), new MessageGate());
    }

    public static Progress console() {
        var lastRenderedState = new AtomicReference<ConsoleState>();
        var useColor = System.console() != null
                && System.getenv("NO_COLOR") == null
                && !"dumb".equalsIgnoreCase(System.getenv("TERM"));
        return new Progress((message, fraction) -> {
            var percent = Math.max(0, Math.min(100, (int) Math.round(fraction * 100.0)));
            // Engine stages are keyed by stage name only, so per-file updates and platforms
            // in the same stage do not print a line each
            Object key = message instanceof Message.BuildingEngineStage stage ? stage.stage() : message;
            ConsoleState renderedState = new ConsoleState(key, percent / 5);
            if (!Objects.equals(lastRenderedState.getAndSet(renderedState), renderedState)) {
                var label = switch (message) {
                    case Message.Bundling _ -> "Bundling";
                    case Message.BuildingEngine _ -> "Building engine";
                    case Message.BuildingEngineStage stage -> "Building engine [" + stage.platform() + "] " + stage.label();
                    case Message.CleaningEngine _ -> "Cleaning engine";
                    case Message.DownloadingSymbols _ -> "Downloading symbols";
                    case Message.TranspilingToLua _ -> "Transpiling to Lua";
                    case Message.ReadingTasks _ -> "Reading tasks";
                    case Message.Building _ -> "Building";
                    case Message.Cleaning _ -> "Cleaning";
                    case Message.GeneratingReport _ -> "Generating report";
                    case Message.Working _ -> "Working";
                    case Message.ReadingClasses _ -> "Reading classes";
                    case Message.DownloadingArchives(var count) -> "Downloading " + count + " archives";
                    case Message.DownloadingArchive(var uri) -> "Downloading " + uri;
                };
                if (useColor) {
                    System.out.printf("%s%3d%%%s %s%s%s%n",
                            "\u001B[38;2;0;163;224m",
                            percent,
                            "\u001B[0m",
                            "\u001B[1m\u001B[38;2;255;143;0m",
                            label,
                            "\u001B[0m");
                } else {
                    System.out.printf("%3d%% %s%n", percent, label);
                }
            }
        });
    }

    @Override
    public void message(Message message) {
        Message nextMessage = Objects.requireNonNull(message);
        if (updateState(state, currentState -> currentState.withMessage(nextMessage)).changed()) {
            report();
        }
    }

    @Override
    public void close() {
        if (updateState(state, ProgressState::close).changed()) {
            report();
        }
    }

    @Override
    public ISplit split(long parts) {
        return new Split(this, parts);
    }

    @Override
    public boolean isCanceled() {
        return reporter.isCanceled();
    }

    private Rational consume(Rational requestedCapacity) {
        StateTransition<ProgressState> transition = updateState(state, currentState -> currentState.consume(requestedCapacity));
        if (transition.changed()) {
            report();
        }
        return transition.newState().completed.subtract(transition.oldState().completed);
    }

    // State updates are atomic but the reporter callback is not, so threads may reach
    // this point out of order. Reporting the current state under a lock rather than the
    // state each thread produced keeps fractions monotonic and the message up to date.
    private void report() {
        synchronized (reportLock) {
            if (reporterClosed) {
                return;
            }
            ProgressState current = state.get();
            if (current == lastReportedState) {
                return;
            }
            ProgressState previous = lastReportedState;
            lastReportedState = current;
            if (!current.closed) {
                reporter.report(current.message, current.completed.doubleValue());
                return;
            }
            reporterClosed = true;
            try (reporter) {
                if (previous == null || !previous.completed.equals(current.completed)) {
                    reporter.report(current.message, current.completed.doubleValue());
                }
            }
        }
    }

    private static Rational totalCapacity(IProgress progress) {
        return switch (progress) {
            case Progress _ -> Rational.ONE;
            case SubProgress subProgress -> subProgress.totalCapacity;
            default -> throw new IllegalStateException("Unsupported progress parent");
        };
    }

    private static Rational consume(IProgress progress, Rational requestedCapacity) {
        return switch (progress) {
            case Progress parent -> parent.consume(requestedCapacity);
            case SubProgress parent -> parent.consume(requestedCapacity, false);
            default -> throw new IllegalStateException("Unsupported progress parent");
        };
    }

    private record Split(IProgress parent, long parts) implements ISplit {
        private Split(IProgress parent, long parts) {
            this.parent = parent;
            this.parts = Math.max(0L, parts);
        }

        @Override
        public void worked(long requestedParts) {
            Progress.consume(parent, capacityForParts(requestedParts));
        }

        @Override
        public IProgress subtask(long requestedParts) {
            return new SubProgress(parent,  capacityForParts(requestedParts));
        }

        private Rational capacityForParts(long requestedParts) {
            Rational totalCapacity = Progress.totalCapacity(parent);
            if (requestedParts <= 0L || parts <= 0L || totalCapacity.isZero()) {
                return Rational.ZERO;
            }
            if (requestedParts >= parts) {
                return totalCapacity;
            }
            return totalCapacity.multiply(requestedParts).divide(parts);
        }
    }

    private static final class MessageGate {
        private boolean released;
        private Message latest;

        // Forwarding under the lock keeps a concurrent release from emitting an older message last
        private synchronized void message(SubProgress owner, Message message) {
            latest = message;
            if (released && !owner.isClosed()) {
                owner.parent.message(message);
            }
        }

        private synchronized void release(SubProgress owner) {
            if (released) {
                return;
            }
            released = true;
            if (latest != null && !owner.isClosed()) {
                owner.parent.message(latest);
            }
        }
    }

    private static final class SubProgress implements Deferred {
        private final IProgress parent;
        private final Rational totalCapacity;
        private final MessageGate gate;
        private final AtomicReference<SubProgressState> state;
        private volatile boolean closed;

        private SubProgress(IProgress parent, Rational totalCapacity) {
            this(parent, totalCapacity, null);
        }

        private SubProgress(IProgress parent, Rational totalCapacity, MessageGate gate) {
            this.parent = parent;
            this.totalCapacity = totalCapacity;
            this.gate = gate;
            this.state = new AtomicReference<>(new SubProgressState(this.totalCapacity, false));
        }

        @Override
        public void message(Message message) {
            if (isClosed()) {
                return;
            }
            if (gate != null) {
                gate.message(this, message);
            } else {
                parent.message(message);
            }
        }

        @Override
        public void releaseMessages() {
            if (gate != null) {
                gate.release(this);
            }
        }

        private boolean isClosed() {
            return closed;
        }

        @Override
        public void close() {
            closed = true;
            consume(totalCapacity, true);
        }

        @Override
        public ISplit split(long parts) {
            return new Split(this, parts);
        }

        @Override
        public boolean isCanceled() {
            return parent.isCanceled();
        }

        private Rational consume(Rational requestedCapacity, boolean closeAfter) {
            var transition = updateState(state, currentState -> currentState.consume(requestedCapacity, closeAfter));
            var reservedCapacity = transition.oldState().remainingCapacity.subtract(transition.newState().remainingCapacity);
            if (reservedCapacity.isZero()) {
                return Rational.ZERO;
            }
            return Progress.consume(parent, reservedCapacity);
        }
    }

    private record ConsoleState(Object key, int bucket) {
    }

    private record StateTransition<T>(T oldState, T newState) {
        private boolean changed() {
            return oldState != newState;
        }
    }

    private record ProgressState(Rational completed, Message message, boolean closed) {

        private ProgressState withMessage(Message message) {
            if (closed || this.message.equals(message)) {
                return this;
            }
            return new ProgressState(completed, message, false);
        }

        private ProgressState consume(Rational requestedCapacity) {
            if (closed || requestedCapacity.isZero() || completed.equals(Rational.ONE)) {
                return this;
            }
            return new ProgressState(completed.add(Rational.ONE.subtract(completed).min(requestedCapacity)), message, false);
        }

        private ProgressState close() {
            if (closed) {
                return this;
            }
            return new ProgressState(Rational.ONE, message, true);
        }
    }

    private record SubProgressState(Rational remainingCapacity, boolean closed) {
        private SubProgressState consume(Rational requestedCapacity, boolean closeAfter) {
            if (closed) {
                return this;
            }
            var reservedCapacity = closeAfter ? remainingCapacity : remainingCapacity.min(requestedCapacity);
            var nextClosed = closeAfter || remainingCapacity.equals(reservedCapacity);
            if (reservedCapacity.isZero()) {
                if (!nextClosed) {
                    return this;
                }
                return new SubProgressState(remainingCapacity, true);
            }
            return new SubProgressState(remainingCapacity.subtract(reservedCapacity), nextClosed);
        }
    }

    private static <T> StateTransition<T> updateState(AtomicReference<T> state, UnaryOperator<T> update) {
        while (true) {
            var oldState = state.get();
            var newState = update.apply(oldState);
            if (state.compareAndSet(oldState, newState)) {
                return new StateTransition<>(oldState, newState);
            }
        }
    }

    private static final class Rational extends Number implements Comparable<Rational> {
        private static final MathContext DOUBLE_PRECISION = MathContext.DECIMAL64;
        private static final Rational ZERO = new Rational(BigInteger.ZERO, BigInteger.ONE);
        private static final Rational ONE = new Rational(BigInteger.ONE, BigInteger.ONE);

        private final BigInteger numerator;
        private final BigInteger denominator;

        private Rational(BigInteger numerator, BigInteger denominator) {
            if (denominator.signum() == 0) {
                throw new IllegalArgumentException("Denominator must be non-zero");
            }
            if (denominator.signum() < 0) {
                numerator = numerator.negate();
                denominator = denominator.negate();
            }
            var gcd = numerator.gcd(denominator);
            this.numerator = numerator.divide(gcd);
            this.denominator = denominator.divide(gcd);
        }

        private Rational add(Rational other) {
            return new Rational(
                    numerator.multiply(other.denominator).add(other.numerator.multiply(denominator)),
                    denominator.multiply(other.denominator));
        }

        private Rational subtract(Rational other) {
            return new Rational(
                    numerator.multiply(other.denominator).subtract(other.numerator.multiply(denominator)),
                    denominator.multiply(other.denominator));
        }

        private Rational multiply(long factor) {
            if (isZero()) {
                return ZERO;
            }
            return new Rational(numerator.multiply(BigInteger.valueOf(factor)), denominator);
        }

        private Rational divide(long divisor) {
            if (isZero()) {
                return ZERO;
            }
            return new Rational(numerator, denominator.multiply(BigInteger.valueOf(divisor)));
        }

        private Rational min(Rational other) {
            return compareTo(other) <= 0 ? this : other;
        }

        private boolean isZero() {
            return numerator.signum() == 0;
        }

        @Override
        public int intValue() {
            return (int) doubleValue();
        }

        @Override
        public long longValue() {
            return (long) doubleValue();
        }

        @Override
        public float floatValue() {
            return (float) doubleValue();
        }

        @Override
        public double doubleValue() {
            return new BigDecimal(numerator).divide(new BigDecimal(denominator), DOUBLE_PRECISION).doubleValue();
        }

        @Override
        public int compareTo(Rational other) {
            return numerator.multiply(other.denominator).compareTo(other.numerator.multiply(denominator));
        }

        @Override
        public boolean equals(Object object) {
            if (this == object) {
                return true;
            }
            if (!(object instanceof Rational other)) {
                return false;
            }
            return numerator.equals(other.numerator) && denominator.equals(other.denominator);
        }

        @Override
        public int hashCode() {
            return Objects.hash(numerator, denominator);
        }
    }
}
