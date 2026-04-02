package com.novaai.compiler.validation;

import com.novaai.compiler.ast.*;
import java.util.*;

/**
 * Standalone test suite for the NOVA Interface Validator.
 * Does not require JUnit - uses simple assertions and prints results.
 */
public class InterfaceValidatorStandaloneTest {
    
    private static int testsRun = 0;
    private static int testsPassed = 0;
    private static int testsFailed = 0;

    public static void main(String[] args) {
        System.out.println("════════════════════════════════════════════════════════════════");
        System.out.println("NOVA Interface Validator Test Suite (Standalone)");
        System.out.println("════════════════════════════════════════════════════════════════\n");

        test_declare_single_interface();
        test_declare_multiple_interfaces();
        test_duplicate_interface_declaration_error();
        test_declare_simple_struct();
        test_declare_struct_with_interface();
        test_interface_not_found_error();
        test_missing_mission_implementation_error();
        test_all_missions_implemented_valid();
        test_partial_mission_implementation_error();
        test_mission_signature_mismatch_error();
        test_mission_signature_match_valid();
        test_multiple_structs_same_interface();
        test_struct_with_fields();
        test_complex_interface_many_missions();

        printSummary();
    }

    private static void assertEquals(Object expected, Object actual, String testName) {
        if (expected == null && actual == null) return;
        if (expected != null && expected.equals(actual)) return;
        fail(testName, "Expected: " + expected + ", Got: " + actual);
    }

    private static void assertTrue(boolean condition, String testName) {
        if (condition) return;
        fail(testName, "Assertion failed: expected true but got false");
    }

    private static void assertFalse(boolean condition, String testName) {
        if (!condition) return;
        fail(testName, "Assertion failed: expected false but got true");
    }

    private static void pass(String testName) {
        System.out.println("✓ " + testName);
        testsPassed++;
        testsRun++;
    }

    private static void fail(String testName, String reason) {
        System.out.println("✗ " + testName);
        System.out.println("  → " + reason);
        testsFailed++;
        testsRun++;
    }

    private static void printSummary() {
        System.out.println("\n════════════════════════════════════════════════════════════════");
        System.out.println("Test Results:");
        System.out.println("  Passed: " + testsPassed);
        System.out.println("  Failed: " + testsFailed);
        System.out.println("  Total:  " + testsRun);
        System.out.println("════════════════════════════════════════════════════════════════");
        
        if (testsFailed == 0) {
            System.out.println("\n✓ All tests passed!");
        } else {
            System.out.println("\n✗ Some tests failed!");
        }
    }

    // ────────────────────────────────────────────────────────────────────────────

    private static void test_declare_single_interface() {
        InterfaceValidator validator = new InterfaceValidator();
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 10, 1);
        validator.declareInterface(drawable);

        assertTrue(validator.getInterface("Drawable").isPresent(), 
                   "test_declare_single_interface: Should find interface");
        pass("declare_single_interface");
    }

    private static void test_declare_multiple_interfaces() {
        InterfaceValidator validator = new InterfaceValidator();
        validator.declareInterface(new InterfaceDeclaration("Drawable", 1, 1));
        validator.declareInterface(new InterfaceDeclaration("Serializable", 2, 1));
        validator.declareInterface(new InterfaceDeclaration("Comparable", 3, 1));

        assertTrue(validator.getInterface("Drawable").isPresent(), 
                   "test_declare_multiple_interfaces: Should find Drawable");
        assertTrue(validator.getInterface("Serializable").isPresent(),
                   "test_declare_multiple_interfaces: Should find Serializable");
        assertTrue(validator.getInterface("Comparable").isPresent(),
                   "test_declare_multiple_interfaces: Should find Comparable");
        pass("declare_multiple_interfaces");
    }

    private static void test_duplicate_interface_declaration_error() {
        InterfaceValidator validator = new InterfaceValidator();
        validator.declareInterface(new InterfaceDeclaration("IFace", 1, 1));
        validator.declareInterface(new InterfaceDeclaration("IFace", 2, 1));

        assertEquals(1, validator.getErrorCount(),
                     "test_duplicate_interface_declaration_error: Should have 1 error");
        pass("duplicate_interface_declaration_error");
    }

    private static void test_declare_simple_struct() {
        InterfaceValidator validator = new InterfaceValidator();
        StructDeclaration vec = new StructDeclaration("Vector3", Optional.empty(), Optional.empty(), 5, 1);
        validator.declareStruct(vec);

        assertTrue(validator.getStruct("Vector3").isPresent(),
                   "test_declare_simple_struct: Should find struct");
        pass("declare_simple_struct");
    }

    private static void test_declare_struct_with_interface() {
        InterfaceValidator validator = new InterfaceValidator();
        validator.declareInterface(new InterfaceDeclaration("Drawable", 1, 1));
        StructDeclaration shape = new StructDeclaration("Circle", Optional.empty(),
                                                        Optional.of("Drawable"), 10, 1);
        validator.declareStruct(shape);

        assertTrue(validator.getStruct("Circle").isPresent(),
                   "test_declare_struct_with_interface: Should find struct");
        pass("declare_struct_with_interface");
    }

    private static void test_interface_not_found_error() {
        InterfaceValidator validator = new InterfaceValidator();
        StructDeclaration shape = new StructDeclaration("Circle", Optional.empty(),
                                                        Optional.of("NonExistentInterface"), 10, 1);
        validator.declareStruct(shape);
        validator.validate();

        assertEquals(1, validator.getErrorCount(),
                     "test_interface_not_found_error: Should have 1 error");
        assertEquals(ValidationError.ErrorType.INTERFACE_NOT_FOUND,
                     validator.getErrors().get(0).type,
                     "test_interface_not_found_error: Error type should be INTERFACE_NOT_FOUND");
        pass("interface_not_found_error");
    }

    private static void test_missing_mission_implementation_error() {
        InterfaceValidator validator = new InterfaceValidator();
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("render", new String[]{}, "Void", 2, 1));
        validator.declareInterface(drawable);

        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 1);
        validator.declareStruct(circle);
        validator.validate();

        assertEquals(1, validator.getErrorCount(),
                     "test_missing_mission_implementation_error: Should have 1 error");
        pass("missing_mission_implementation_error");
    }

    private static void test_all_missions_implemented_valid() {
        InterfaceValidator validator = new InterfaceValidator();
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("render", new String[]{}, "Void", 2, 1));
        drawable.addMission(new MissionSignature("update", new String[]{"Float"}, "Void", 3, 1));
        validator.declareInterface(drawable);

        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 1);
        circle.addImplementedMission("render");
        circle.addImplementedMission("update");
        validator.declareStruct(circle);
        validator.validate();

        assertTrue(validator.isValid(),
                   "test_all_missions_implemented_valid: Should be valid");
        pass("all_missions_implemented_valid");
    }

    private static void test_partial_mission_implementation_error() {
        InterfaceValidator validator = new InterfaceValidator();
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("render", new String[]{}, "Void", 2, 1));
        drawable.addMission(new MissionSignature("update", new String[]{"Float"}, "Void", 3, 1));
        drawable.addMission(new MissionSignature("destroy", new String[]{}, "Void", 4, 1));
        validator.declareInterface(drawable);

        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 1);
        circle.addImplementedMission("render");
        circle.addImplementedMission("update");
        validator.declareStruct(circle);
        validator.validate();

        assertEquals(1, validator.getErrorCount(),
                     "test_partial_mission_implementation_error: Should have 1 error");
        pass("partial_mission_implementation_error");
    }

    private static void test_mission_signature_mismatch_error() {
        InterfaceValidator validator = new InterfaceValidator();
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("render", new String[]{"Float", "Int"}, "Void", 2, 1));
        validator.declareInterface(drawable);

        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 1);
        circle.addImplementedMission("render");
        validator.declareStruct(circle);
        validator.validateMissionImplementation("Circle",
            new MissionSignature("render", new String[]{"Float"}, "Void", 11, 1));
        
        assertEquals(1, validator.getErrorCount(),
                     "test_mission_signature_mismatch_error: Should have 1 error");
        pass("mission_signature_mismatch_error");
    }

    private static void test_mission_signature_match_valid() {
        InterfaceValidator validator = new InterfaceValidator();
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("render", new String[]{"Float", "Int"}, "Void", 2, 1));
        validator.declareInterface(drawable);

        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 1);
        circle.addImplementedMission("render");
        validator.declareStruct(circle);
        validator.validateMissionImplementation("Circle",
            new MissionSignature("render", new String[]{"Float", "Int"}, "Void", 11, 1));

        assertTrue(validator.isValid(),
                   "test_mission_signature_match_valid: Should be valid");
        pass("mission_signature_match_valid");
    }

    private static void test_multiple_structs_same_interface() {
        InterfaceValidator validator = new InterfaceValidator();
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("render", new String[]{}, "Void", 2, 1));
        validator.declareInterface(drawable);

        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 1);
        circle.addImplementedMission("render");
        validator.declareStruct(circle);

        StructDeclaration square = new StructDeclaration("Square", Optional.empty(),
                                                         Optional.of("Drawable"), 20, 1);
        square.addImplementedMission("render");
        validator.declareStruct(square);

        validator.validate();
        assertTrue(validator.isValid(),
                   "test_multiple_structs_same_interface: Should be valid");
        pass("multiple_structs_same_interface");
    }

    private static void test_struct_with_fields() {
        InterfaceValidator validator = new InterfaceValidator();
        StructDeclaration vec = new StructDeclaration("Vector3", Optional.empty(), Optional.empty(), 5, 1);
        vec.addField(new StructField("x", "Float[m]", 6, 5));
        vec.addField(new StructField("y", "Float[m]", 7, 5));
        vec.addField(new StructField("z", "Float[m]", 8, 5));
        validator.declareStruct(vec);

        StructDeclaration retrieved = validator.getStruct("Vector3").get();
        assertEquals(3, retrieved.fields.size(),
                     "test_struct_with_fields: Should have 3 fields");
        pass("struct_with_fields");
    }

    private static void test_complex_interface_many_missions() {
        InterfaceValidator validator = new InterfaceValidator();
        InterfaceDeclaration processor = new InterfaceDeclaration("DataProcessor", 1, 1);
        processor.addMission(new MissionSignature("init", new String[]{}, "Void", 2, 1));
        processor.addMission(new MissionSignature("process", new String[]{"Array[Float]"}, "Array[Float]", 3, 1));
        processor.addMission(new MissionSignature("finalize", new String[]{}, "Void", 4, 1));
        processor.addMission(new MissionSignature("getStatus", new String[]{}, "Str", 5, 1));
        validator.declareInterface(processor);

        StructDeclaration impl = new StructDeclaration("MyProcessor", Optional.empty(),
                                                       Optional.of("DataProcessor"), 10, 1);
        impl.addImplementedMission("init");
        impl.addImplementedMission("process");
        impl.addImplementedMission("finalize");
        impl.addImplementedMission("getStatus");
        validator.declareStruct(impl);
        validator.validate();

        assertTrue(validator.isValid(),
                   "test_complex_interface_many_missions: Should be valid");
        pass("complex_interface_many_missions");
    }
}
