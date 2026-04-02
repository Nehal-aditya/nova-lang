package com.novaai.compiler.ast;

import java.util.*;

/**
 * Represents an interface declaration: a contract that structs can implement.
 * Contains a set of mission signatures that implementing structs must provide.
 */
public class InterfaceDeclaration {
    public final String name;
    public final List<MissionSignature> missions;
    public final int line;
    public final int col;

    public InterfaceDeclaration(String name, int line, int col) {
        this.name = name;
        this.missions = new ArrayList<>();
        this.line = line;
        this.col = col;
    }

    public void addMission(MissionSignature mission) {
        this.missions.add(mission);
    }

    public Optional<MissionSignature> getMission(String name) {
        return missions.stream()
            .filter(m -> m.name.equals(name))
            .findFirst();
    }

    @Override
    public String toString() {
        return "interface " + name + " { " + missions.size() + " missions } [" + line + ":" + col + "]";
    }
}
