package com.novaai.compiler.validation;

import com.novaai.compiler.ast.*;
import java.util.*;

/**
 * The main NOVA Interface Validator.
 * 
 * Validates that structs correctly implement their declared interfaces.
 * Performs:
 *   1. Interface existence check: interface must be declared
 *   2. Mission completeness check: all interface missions must be implemented
 *   3. Signature matching: implemented missions must match interface signatures
 *   4. Duplicate detection: no duplicate interface/struct declarations
 */
public class InterfaceValidator {
    private final Map<String, InterfaceDeclaration> interfaces;
    private final Map<String, StructDeclaration> structs;
    private final List<ValidationError> errors;

    public InterfaceValidator() {
        this.interfaces = new HashMap<>();
        this.structs = new HashMap<>();
        this.errors = new ArrayList<>();
    }

    /**
     * Register an interface declaration.
     */
    public void declareInterface(InterfaceDeclaration iface) {
        if (interfaces.containsKey(iface.name)) {
            ValidationError err = new ValidationError(
                ValidationError.ErrorType.DUPLICATE_INTERFACE,
                "interface '" + iface.name + "' already declared",
                iface.line, iface.col,
                "", iface.name
            );
            errors.add(err);
        } else {
            interfaces.put(iface.name, iface);
        }
    }

    /**
     * Register a struct declaration.
     */
    public void declareStruct(StructDeclaration struct) {
        if (structs.containsKey(struct.name)) {
            ValidationError err = new ValidationError(
                ValidationError.ErrorType.DUPLICATE_STRUCT,
                "struct '" + struct.name + "' already declared",
                struct.line, struct.col,
                struct.name, ""
            );
            errors.add(err);
        } else {
            structs.put(struct.name, struct);
        }
    }

    /**
     * Validate that a struct correctly implements its declared interface.
     * Called after all declarations are registered.
     */
    public void validateStruct(StructDeclaration struct) {
        // If struct doesn't implement an interface, nothing to validate
        if (!struct.implementsInterface.isPresent()) {
            return;
        }

        String interfaceName = struct.implementsInterface.get();
        InterfaceDeclaration iface = interfaces.get(interfaceName);

        // Check: interface must exist
        if (iface == null) {
            ValidationError err = new ValidationError(
                ValidationError.ErrorType.INTERFACE_NOT_FOUND,
                "interface '" + interfaceName + "' not found",
                struct.line, struct.col,
                struct.name, interfaceName
            );
            errors.add(err);
            return;
        }

        // Check: all interface missions must be implemented
        for (MissionSignature interfaceMission : iface.missions) {
            if (!struct.implementedMissions.contains(interfaceMission.name)) {
                ValidationError err = new ValidationError(
                    ValidationError.ErrorType.MISSING_MISSION,
                    "missing implementation of mission '" + interfaceMission.name + "'",
                    struct.line, struct.col,
                    struct.name, interfaceName
                );
                errors.add(err);
            }
        }
    }

    /**
     * Validate that an implemented mission matches the interface signature.
     */
    public void validateMissionImplementation(String structName, 
                                             MissionSignature implementedMission) {
        StructDeclaration struct = structs.get(structName);
        if (struct == null || !struct.implementsInterface.isPresent()) {
            return;
        }

        String interfaceName = struct.implementsInterface.get();
        InterfaceDeclaration iface = interfaces.get(interfaceName);
        if (iface == null) {
            return;  // Will be caught by validateStruct
        }

        Optional<MissionSignature> interfaceMission = iface.getMission(implementedMission.name);
        if (!interfaceMission.isPresent()) {
            // Mission not in interface: extra implementation, not an error per se
            return;
        }

        // Validate signature match
        MissionSignature expected = interfaceMission.get();
        if (!expected.matches(implementedMission)) {
            ValidationError err = new ValidationError(
                ValidationError.ErrorType.SIGNATURE_MISMATCH,
                "mission '" + implementedMission.name + "' signature mismatch: " +
                "expected " + expected.getSignatureString() + ", " +
                "got " + implementedMission.getSignatureString(),
                implementedMission.line, implementedMission.col,
                structName, interfaceName
            );
            errors.add(err);
        }
    }

    /**
     * Run full validation pass: interfaces exist, structs are complete, signatures match.
     */
    public void validate() {
        // Clear previous errors
        errors.clear();

        // Validate all structs that implement interfaces
        for (StructDeclaration struct : structs.values()) {
            validateStruct(struct);
        }
    }

    /**
     * Check if validation succeeded (no errors).
     */
    public boolean isValid() {
        return errors.isEmpty();
    }

    /**
     * Get all accumulated validation errors.
     */
    public List<ValidationError> getErrors() {
        return new ArrayList<>(errors);
    }

    /**
     * Get error count.
     */
    public int getErrorCount() {
        return errors.size();
    }

    /**
     * Get all registered interfaces.
     */
    public Map<String, InterfaceDeclaration> getInterfaces() {
        return new HashMap<>(interfaces);
    }

    /**
     * Get all registered structs.
     */
    public Map<String, StructDeclaration> getStructs() {
        return new HashMap<>(structs);
    }

    /**
     * Get a specific interface by name.
     */
    public Optional<InterfaceDeclaration> getInterface(String name) {
        return Optional.ofNullable(interfaces.get(name));
    }

    /**
     * Get a specific struct by name.
     */
    public Optional<StructDeclaration> getStruct(String name) {
        return Optional.ofNullable(structs.get(name));
    }

    /**
     * Clear all declarations and errors (reset state).
     */
    public void clear() {
        interfaces.clear();
        structs.clear();
        errors.clear();
    }
}
