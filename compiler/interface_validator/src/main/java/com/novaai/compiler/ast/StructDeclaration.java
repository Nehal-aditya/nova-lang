package com.novaai.compiler.ast;

import java.util.*;

/**
 * Represents a struct declaration: a data type that may implement an interface.
 * Contains the struct's name, optional base struct, optional interface implementation,
 * and the fields it declares.
 */
public class StructDeclaration {
    public final String name;
    public final Optional<String> baseStruct;
    public final Optional<String> implementsInterface;
    public final List<StructField> fields;
    public final Set<String> implementedMissions;
    public final int line;
    public final int col;

    public StructDeclaration(String name, Optional<String> baseStruct, Optional<String> implementsInterface, int line, int col) {
        this.name = name;
        this.baseStruct = baseStruct;
        this.implementsInterface = implementsInterface;
        this.fields = new ArrayList<>();
        this.implementedMissions = new HashSet<>();
        this.line = line;
        this.col = col;
    }

    public void addField(StructField field) {
        this.fields.add(field);
    }

    public void addImplementedMission(String missionName) {
        this.implementedMissions.add(missionName);
    }

    public Optional<StructField> getField(String fieldName) {
        return fields.stream()
            .filter(f -> f.name.equals(fieldName))
            .findFirst();
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("struct ").append(name);
        if (baseStruct.isPresent()) {
            sb.append(" extends ").append(baseStruct.get());
        }
        if (implementsInterface.isPresent()) {
            sb.append(" implements ").append(implementsInterface.get());
        }
        sb.append(" { ").append(fields.size()).append(" fields }");
        sb.append(" [").append(line).append(":").append(col).append("]");
        return sb.toString();
    }
}
