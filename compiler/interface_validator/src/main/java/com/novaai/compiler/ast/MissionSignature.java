package com.novaai.compiler.ast;

/**
 * Represents a mission (function) signature: name and parameter types.
 * Used to define interface contracts and validate implementations.
 */
public class MissionSignature {
    public final String name;
    public final String[] paramTypes;
    public final String returnType;
    public final int line;
    public final int col;

    public MissionSignature(String name, String[] paramTypes, String returnType, int line, int col) {
        this.name = name;
        this.paramTypes = paramTypes;
        this.returnType = returnType;
        this.line = line;
        this.col = col;
    }

    /**
     * Check if this signature matches another (same name, same param count and types).
     */
    public boolean matches(MissionSignature other) {
        if (!this.name.equals(other.name)) {
            return false;
        }
        if (this.paramTypes.length != other.paramTypes.length) {
            return false;
        }
        for (int i = 0; i < this.paramTypes.length; i++) {
            if (!this.paramTypes[i].equals(other.paramTypes[i])) {
                return false;
            }
        }
        return true;
    }

    public String getSignatureString() {
        StringBuilder sb = new StringBuilder();
        sb.append(name).append("(");
        for (int i = 0; i < paramTypes.length; i++) {
            if (i > 0) sb.append(", ");
            sb.append(paramTypes[i]);
        }
        sb.append(") -> ").append(returnType);
        return sb.toString();
    }

    @Override
    public String toString() {
        return getSignatureString() + " [" + line + ":" + col + "]";
    }
}
