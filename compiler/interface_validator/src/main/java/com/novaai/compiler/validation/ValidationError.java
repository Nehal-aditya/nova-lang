package com.novaai.compiler.validation;

/**
 * Validation error from the interface validator.
 * Represents a problem with a struct's interface implementation.
 */
public class ValidationError {
    public enum ErrorType {
        INTERFACE_NOT_FOUND("interface not found"),
        MISSING_MISSION("missing required mission implementation"),
        SIGNATURE_MISMATCH("mission signature does not match interface"),
        INVALID_MISSION_COUNT("incorrect number of missions implemented"),
        DUPLICATE_STRUCT("struct declared multiple times"),
        DUPLICATE_INTERFACE("interface declared multiple times");

        private final String description;

        ErrorType(String description) {
            this.description = description;
        }

        public String getDescription() {
            return description;
        }
    }

    public final ErrorType type;
    public final String message;
    public final int line;
    public final int col;
    public final String structName;
    public final String interfaceName;

    public ValidationError(ErrorType type, String message, int line, int col, 
                          String structName, String interfaceName) {
        this.type = type;
        this.message = message;
        this.line = line;
        this.col = col;
        this.structName = structName;
        this.interfaceName = interfaceName;
    }

    @Override
    public String toString() {
        return String.format("%d:%d: %s: %s (struct '%s'%s)",
            line, col,
            type.getDescription(),
            message,
            structName,
            interfaceName != null && !interfaceName.isEmpty() ? 
                " implementing '" + interfaceName + "'" : "");
    }
}
