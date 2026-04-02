package com.novaai.compiler.validation;

import com.novaai.compiler.ast.*;
import org.junit.jupiter.api.*;

import java.util.Optional;

import static org.junit.jupiter.api.Assertions.*;

/**
 * Comprehensive test suite for the NOVA Interface Validator.
 */
public class InterfaceValidatorTest {

    private InterfaceValidator validator;

    @BeforeEach
    public void setUp() {
        validator = new InterfaceValidator();
    }

    // ── Basic interface declaration ────────────────────────────────────────────

    @Test
    public void declare_single_interface() {
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 10, 1);
        validator.declareInterface(drawable);

        assertTrue(validator.getInterface("Drawable").isPresent());
        assertEquals("Drawable", validator.getInterface("Drawable").get().name);
    }

    @Test
    public void declare_multiple_interfaces() {
        validator.declareInterface(new InterfaceDeclaration("Drawable", 1, 1));
        validator.declareInterface(new InterfaceDeclaration("Serializable", 2, 1));
        validator.declareInterface(new InterfaceDeclaration("Comparable", 3, 1));

        assertTrue(validator.getInterface("Drawable").isPresent());
        assertTrue(validator.getInterface("Serializable").isPresent());
        assertTrue(validator.getInterface("Comparable").isPresent());
    }

    @Test
    public void duplicate_interface_declaration_error() {
        validator.declareInterface(new InterfaceDeclaration("IFace", 1, 1));
        validator.declareInterface(new InterfaceDeclaration("IFace", 2, 1));

        assertEquals(1, validator.getErrorCount());
        assertEquals(ValidationError.ErrorType.DUPLICATE_INTERFACE,
                validator.getErrors().get(0).type);
    }

    // ── Basic struct declaration ───────────────────────────────────────────────

    @Test
    public void declare_simple_struct() {
        StructDeclaration vec = new StructDeclaration("Vector3", Optional.empty(), Optional.empty(), 5, 1);
        validator.declareStruct(vec);

        assertTrue(validator.getStruct("Vector3").isPresent());
    }

    @Test
    public void declare_struct_with_interface() {
        validator.declareInterface(new InterfaceDeclaration("Drawable", 1, 1));
        StructDeclaration shape = new StructDeclaration("Circle", Optional.empty(), 
                                                        Optional.of("Drawable"), 10, 1);
        validator.declareStruct(shape);

        assertTrue(validator.getStruct("Circle").isPresent());
    }

    @Test
    public void duplicate_struct_declaration_error() {
        validator.declareStruct(new StructDeclaration("MyStruct", Optional.empty(), Optional.empty(), 1, 1));
        validator.declareStruct(new StructDeclaration("MyStruct", Optional.empty(), Optional.empty(), 2, 1));

        assertEquals(1, validator.getErrorCount());
        assertEquals(ValidationError.ErrorType.DUPLICATE_STRUCT,
                validator.getErrors().get(0).type);
    }

    // ── Interface existence ────────────────────────────────────────────────────

    @Test
    public void interface_not_found_error() {
        StructDeclaration shape = new StructDeclaration("Circle", Optional.empty(),
                                                        Optional.of("NonExistentInterface"), 10, 1);
        validator.declareStruct(shape);
        validator.validate();

        assertEquals(1, validator.getErrorCount());
        assertEquals(ValidationError.ErrorType.INTERFACE_NOT_FOUND,
                validator.getErrors().get(0).type);
    }

    // ── Mission implementation ─────────────────────────────────────────────────

    @Test
    public void struct_with_no_interface_declaration_valid() {
        StructDeclaration vec = new StructDeclaration("Vector3", Optional.empty(), Optional.empty(), 5, 1);
        validator.declareStruct(vec);
        validator.validate();

        assertTrue(validator.isValid());
    }

    @Test
    public void missing_mission_implementation_error() {
        // Create interface with a mission
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("render", new String[]{}, "Void", 2, 1));
        validator.declareInterface(drawable);

        // Create struct that claims to implement but doesn't
        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 1);
        validator.declareStruct(circle);
        validator.validate();

        assertEquals(1, validator.getErrorCount());
        assertEquals(ValidationError.ErrorType.MISSING_MISSION,
                validator.getErrors().get(0).type);
        assertTrue(validator.getErrors().get(0).message.contains("render"));
    }

    @Test
    public void all_missions_implemented_valid() {
        // Create interface
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("render", new String[]{}, "Void", 2, 1));
        drawable.addMission(new MissionSignature("update", new String[]{"Float"}, "Void", 3, 1));
        validator.declareInterface(drawable);

        // Create struct that implements all missions
        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 1);
        circle.addImplementedMission("render");
        circle.addImplementedMission("update");
        validator.declareStruct(circle);
        validator.validate();

        assertTrue(validator.isValid());
        assertEquals(0, validator.getErrorCount());
    }

    @Test
    public void partial_mission_implementation_error() {
        // Create interface with 3 missions
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("render", new String[]{}, "Void", 2, 1));
        drawable.addMission(new MissionSignature("update", new String[]{"Float"}, "Void", 3, 1));
        drawable.addMission(new MissionSignature("destroy", new String[]{}, "Void", 4, 1));
        validator.declareInterface(drawable);

        // Struct implements only 2 missions
        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 1);
        circle.addImplementedMission("render");
        circle.addImplementedMission("update");
        validator.declareStruct(circle);
        validator.validate();

        assertEquals(1, validator.getErrorCount());
        assertTrue(validator.getErrors().get(0).message.contains("destroy"));
    }

    // ── Signature matching ─────────────────────────────────────────────────────

    @Test
    public void mission_signature_mismatch_error() {
        // Create interface with specific signature
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("render", new String[]{"Float", "Int"}, "Void", 2, 1));
        validator.declareInterface(drawable);

        // Create struct with mismatched signature
        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 1);
        circle.addImplementedMission("render");
        validator.declareStruct(circle);
        validator.validateMissionImplementation("Circle", 
            new MissionSignature("render", new String[]{"Float"}, "Void", 11, 1)); // Wrong param count
        
        assertEquals(1, validator.getErrorCount());
        assertEquals(ValidationError.ErrorType.SIGNATURE_MISMATCH,
                validator.getErrors().get(0).type);
    }

    @Test
    public void mission_signature_match_valid() {
        // Create interface
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("render", new String[]{"Float", "Int"}, "Void", 2, 1));
        validator.declareInterface(drawable);

        // Create struct with matching signature
        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 1);
        circle.addImplementedMission("render");
        validator.declareStruct(circle);
        validator.validateMissionImplementation("Circle",
            new MissionSignature("render", new String[]{"Float", "Int"}, "Void", 11, 1));

        assertTrue(validator.isValid());
        assertEquals(0, validator.getErrorCount());
    }

    @Test
    public void wrong_parameter_type_mismatch() {
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("process", new String[]{"Int"}, "Float", 2, 1));
        validator.declareInterface(drawable);

        StructDeclaration processor = new StructDeclaration("Processor", Optional.empty(),
                                                            Optional.of("Drawable"), 10, 1);
        processor.addImplementedMission("process");
        validator.declareStruct(processor);
        validator.validateMissionImplementation("Processor",
            new MissionSignature("process", new String[]{"Str"}, "Float", 11, 1)); // Wrong type

        assertEquals(1, validator.getErrorCount());
        assertTrue(validator.getErrors().get(0).message.contains("Int"));
        assertTrue(validator.getErrors().get(0).message.contains("Str"));
    }

    // ── Complex scenarios ──────────────────────────────────────────────────────

    @Test
    public void multiple_structs_same_interface() {
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("render", new String[]{}, "Void", 2, 1));
        validator.declareInterface(drawable);

        // Struct 1: implements it correctly
        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 1);
        circle.addImplementedMission("render");
        validator.declareStruct(circle);

        // Struct 2: implements it correctly
        StructDeclaration square = new StructDeclaration("Square", Optional.empty(),
                                                         Optional.of("Drawable"), 20, 1);
        square.addImplementedMission("render");
        validator.declareStruct(square);

        validator.validate();
        assertTrue(validator.isValid());
    }

    @Test
    public void multiple_structs_some_fail() {
        InterfaceDeclaration drawable = new InterfaceDeclaration("Drawable", 1, 1);
        drawable.addMission(new MissionSignature("render", new String[]{}, "Void", 2, 1));
        validator.declareInterface(drawable);

        // Struct 1: valid
        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 1);
        circle.addImplementedMission("render");
        validator.declareStruct(circle);

        // Struct 2: missing mission
        StructDeclaration square = new StructDeclaration("Square", Optional.empty(),
                                                         Optional.of("Drawable"), 20, 1);
        validator.declareStruct(square);

        validator.validate();
        assertFalse(validator.isValid());
        assertEquals(1, validator.getErrorCount());
    }

    @Test
    public void struct_with_base_class() {
        StructDeclaration base = new StructDeclaration("Shape", Optional.empty(), Optional.empty(), 5, 1);
        validator.declareStruct(base);

        StructDeclaration circle = new StructDeclaration("Circle", Optional.of("Shape"), Optional.empty(), 10, 1);
        validator.declareStruct(circle);

        assertTrue(validator.getStruct("Shape").isPresent());
        assertTrue(validator.getStruct("Circle").isPresent());
        assertEquals("Shape", validator.getStruct("Circle").get().baseStruct.get());
    }

    @Test
    public void struct_with_fields() {
        StructDeclaration vec = new StructDeclaration("Vector3", Optional.empty(), Optional.empty(), 5, 1);
        vec.addField(new StructField("x", "Float[m]", 6, 5));
        vec.addField(new StructField("y", "Float[m]", 7, 5));
        vec.addField(new StructField("z", "Float[m]", 8, 5));
        validator.declareStruct(vec);

        StructDeclaration retrieved = validator.getStruct("Vector3").get();
        assertEquals(3, retrieved.fields.size());
        assertTrue(retrieved.getField("x").isPresent());
        assertEquals("Float[m]", retrieved.getField("x").get().type);
    }

    // ── Error reporting ───────────────────────────────────────────────────────

    @Test
    public void error_has_location_info() {
        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("NonExistent"), 42, 15);
        validator.declareStruct(circle);
        validator.validate();

        ValidationError err = validator.getErrors().get(0);
        assertEquals(42, err.line);
        assertEquals(15, err.col);
    }

    @Test
    public void error_string_representation() {
        StructDeclaration circle = new StructDeclaration("Circle", Optional.empty(),
                                                         Optional.of("Drawable"), 10, 5);
        validator.declareStruct(circle);
        validator.validate();

        String errStr = validator.getErrors().get(0).toString();
        assertTrue(errStr.contains("Circle"));
        assertTrue(errStr.contains("Drawable"));
        assertTrue(errStr.contains("10:5"));
    }

    // ── State management ───────────────────────────────────────────────────────

    @Test
    public void clear_resets_state() {
        validator.declareInterface(new InterfaceDeclaration("IFace", 1, 1));
        validator.declareStruct(new StructDeclaration("Struct", Optional.empty(), 
                                                      Optional.of("NonExistent"), 10, 1));
        validator.validate();

        assertFalse(validator.isValid());
        validator.clear();
        assertTrue(validator.isValid());
        assertEquals(0, validator.getInterfaces().size());
        assertEquals(0, validator.getStructs().size());
    }

    // ── Complex interface ──────────────────────────────────────────────────────

    @Test
    public void complex_interface_many_missions() {
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

        assertTrue(validator.isValid());
    }

    @Test
    public void multiple_errors_accumulated() {
        InterfaceDeclaration iface1 = new InterfaceDeclaration("Drawable", 1, 1);
        iface1.addMission(new MissionSignature("draw", new String[]{}, "Void", 2, 1));
        validator.declareInterface(iface1);

        // Struct 1: missing mission
        StructDeclaration s1 = new StructDeclaration("Shape1", Optional.empty(),
                                                     Optional.of("Drawable"), 10, 1);
        validator.declareStruct(s1);

        // Struct 2: implements non-existent interface
        StructDeclaration s2 = new StructDeclaration("Shape2", Optional.empty(),
                                                     Optional.of("NotFound"), 20, 1);
        validator.declareStruct(s2);

        validator.validate();
        assertEquals(2, validator.getErrorCount());
    }
}
