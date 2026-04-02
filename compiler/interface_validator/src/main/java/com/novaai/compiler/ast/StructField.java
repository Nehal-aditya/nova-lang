package com.novaai.compiler.ast;

/**
 * Represents a field in a struct: name and type.
 */
public class StructField {
    public final String name;
    public final String type;
    public final int line;
    public final int col;

    public StructField(String name, String type, int line, int col) {
        this.name = name;
        this.type = type;
        this.line = line;
        this.col = col;
    }

    @Override
    public String toString() {
        return name + ": " + type + " [" + line + ":" + col + "]";
    }
}
