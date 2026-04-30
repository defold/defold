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

    private final Reporter reporter;
    private final AtomicReference<ProgressState> state = new AtomicReference<>(new ProgressState(Rational.ZERO, Message.Working.INSTANCE, false));

    public Progress(Reporter reporter) {
        this.reporter = Objects.requireNonNull(reporter);
    }

    public static Progress discarding() {
        return new Progress((_, _) -> {
        });
    }

    public static Progress console() {
        var lastRenderedState = new AtomicReference<ConsoleState>();
        var useColor = System.console() != null
                && System.getenv("NO_COLOR") == null
                && !"dumb".equalsIgnoreCase(System.getenv("TERM"));
        return new Progress((message, fraction) -> {
            var percent = Math.max(0, Math.min(100, (int) Math.round(fraction * 100.0)));
            var renderedState = new ConsoleState(message, percent / 5);
            if (!Objects.equals(lastRenderedState.getAndSet(renderedState), renderedState)) {
                var label = switch (message) {
                    case Message.Bundling _ -> "Bundling";
                    case Message.BuildingEngine _ -> "Building engine";
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
        var nextMessage = Objects.requireNonNull(message);
        var transition = updateState(state, currentState -> currentState.withMessage(nextMessage));
        if (transition.changed() && !transition.newState().closed) {
            report(transition.newState());
        }
    }

    @Override
    public void close() {
        var transition = updateState(state, ProgressState::close);
        if (transition.changed()) {
            try (reporter) {
                if (!transition.oldState().completed.equals(transition.newState().completed)) {
                    report(transition.newState());
                }
            }
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
        var transition = updateState(state, currentState -> currentState.consume(requestedCapacity));
        if (transition.changed()) {
            report(transition.newState());
        }
        return transition.newState().completed.subtract(transition.oldState().completed);
    }

    private void report(ProgressState state) {
        reporter.report(state.message, state.completed.doubleValue());
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
            var totalCapacity = switch (parent) {
                case Progress _ -> Rational.ONE;
                case SubProgress subProgress -> subProgress.totalCapacity;
                default -> throw new IllegalStateException("Unsupported progress parent");
            };
            if (requestedParts <= 0L || parts <= 0L || totalCapacity.isZero()) {
                return Rational.ZERO;
            }
            if (requestedParts >= parts) {
                return totalCapacity;
            }
            return totalCapacity.multiply(requestedParts).divide(parts);
        }
    }

    private static final class SubProgress implements IProgress {
        private final IProgress parent;
        private final Rational totalCapacity;
        private final AtomicReference<SubProgressState> state;

        private SubProgress(IProgress parent,  Rational totalCapacity) {
            this.parent = parent;
            this.totalCapacity = totalCapacity;
            this.state = new AtomicReference<>(new SubProgressState(this.totalCapacity, false));
        }

        @Override
        public void message(Message message) {
            parent.message(message);
        }

        @Override
        public void close() {
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

    private record ConsoleState(Message label, int bucket) {
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
