#![allow(non_upper_case_globals)]

use super::hb::*;

use std::ffi::c_void;
use std::mem::transmute;
use std::ptr::null_mut;
use std::str::FromStr;

use harfrust::{FontRef, NormalizedCoord, Shaper, ShaperData, ShaperInstance, Tag};

pub struct HBHarfRustFaceData<'a> {
    face_blob: *mut hb_blob_t,
    font_ref: FontRef<'a>,
    shaper_data: ShaperData,
}

#[no_mangle]
pub unsafe extern "C" fn _hb_harfrust_shaper_face_data_create_rs(
    face: *mut hb_face_t,
) -> *mut c_void {
    let face_index = hb_face_get_index(face);
    let face_blob = hb_face_reference_blob(face);
    let blob_length = hb_blob_get_length(face_blob);
    let blob_data = hb_blob_get_data(face_blob, null_mut());
    if blob_data.is_null() {
        return null_mut();
    }
    let face_data = std::slice::from_raw_parts(blob_data as *const u8, blob_length as usize);

    let font_ref = match FontRef::from_index(face_data, face_index) {
        Ok(f) => f,
        Err(_) => return null_mut(),
    };
    let shaper_data = ShaperData::new(&font_ref);

    let hr_face_data = Box::new(HBHarfRustFaceData {
        face_blob,
        font_ref,
        shaper_data,
    });

    Box::into_raw(hr_face_data) as *mut c_void
}

#[no_mangle]
pub unsafe extern "C" fn _hb_harfrust_shaper_face_data_destroy_rs(data: *mut c_void) {
    let data = data as *mut HBHarfRustFaceData;
    let hr_face_data = Box::from_raw(data);
    let blob = hr_face_data.face_blob;
    hb_blob_destroy(blob);
}

pub struct HBHarfRustFontData {
    shaper_instance: Box<ShaperInstance>,
    shaper: Shaper<'static>,
}

fn font_to_shaper_instance(font: *mut hb_font_t, font_ref: &FontRef<'_>) -> ShaperInstance {
    let mut num_coords: u32 = 0;
    let coords = unsafe { hb_font_get_var_coords_normalized(font, &mut num_coords) };
    let coords = if coords.is_null() {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(coords, num_coords as usize) }
    };
    let coords = coords.iter().map(|&v| NormalizedCoord::from_bits(v as i16));
    ShaperInstance::from_coords(font_ref, coords)
}

#[no_mangle]
pub unsafe extern "C" fn _hb_harfrust_shaper_font_data_create_rs(
    font: *mut hb_font_t,
    face_data: *const c_void,
) -> *mut c_void {
    let face_data = face_data as *const HBHarfRustFaceData;

    let font_ref = &(*face_data).font_ref;
    let shaper_instance = Box::new(font_to_shaper_instance(font, font_ref));

    let shaper_instance_ref = &*(&*shaper_instance as *const _);
    let shaper = (*face_data)
        .shaper_data
        .shaper(font_ref)
        .instance(Some(shaper_instance_ref))
        .build();

    let hr_font_data = Box::new(HBHarfRustFontData {
        shaper_instance,
        shaper: transmute::<harfrust::Shaper<'_>, harfrust::Shaper<'_>>(shaper),
    });
    let hr_font_data_ptr = Box::into_raw(hr_font_data);

    hr_font_data_ptr as *mut c_void
}

#[no_mangle]
pub unsafe extern "C" fn _hb_harfrust_shaper_font_data_destroy_rs(data: *mut c_void) {
    let data = data as *mut HBHarfRustFontData;
    let _hr_font_data = Box::from_raw(data);
}

fn hb_language_to_hr_language(language: hb_language_t) -> Option<harfrust::Language> {
    let language_str = unsafe { hb_language_to_string(language) };
    if language_str.is_null() {
        return None;
    }
    let language_str = unsafe { std::ffi::CStr::from_ptr(language_str) };
    let language_str = language_str.to_str().unwrap_or_default();
    Some(harfrust::Language::from_str(language_str).unwrap())
}

#[no_mangle]
pub unsafe extern "C" fn _hb_harfrust_buffer_create_rs() -> *mut c_void {
    let hr_buffer = Box::new(harfrust::UnicodeBuffer::new());
    Box::into_raw(hr_buffer) as *mut c_void
}

#[no_mangle]
pub unsafe extern "C" fn _hb_harfrust_buffer_destroy_rs(data: *mut c_void) {
    let data = data as *mut harfrust::UnicodeBuffer;
    let _hr_buffer = Box::from_raw(data);
}

#[no_mangle]
pub unsafe extern "C" fn _hb_harfrust_shape_plan_create_rs(
    font_data: *const c_void,
    script: hb_script_t,
    language: hb_language_t,
    direction: hb_direction_t,
) -> *mut c_void {
    let font_data = font_data as *const HBHarfRustFontData;

    let script = harfrust::Script::from_iso15924_tag(Tag::from_u32(script));
    let language = hb_language_to_hr_language(language);
    let direction = match direction {
        hb_direction_t_HB_DIRECTION_LTR => harfrust::Direction::LeftToRight,
        hb_direction_t_HB_DIRECTION_RTL => harfrust::Direction::RightToLeft,
        hb_direction_t_HB_DIRECTION_TTB => harfrust::Direction::TopToBottom,
        hb_direction_t_HB_DIRECTION_BTT => harfrust::Direction::BottomToTop,
        _ => harfrust::Direction::Invalid,
    };

    let shaper = &(*font_data).shaper;

    let hr_shape_plan = harfrust::ShapePlan::new(shaper, direction, script, language.as_ref(), &[]);
    let hr_shape_plan = Box::new(hr_shape_plan);
    Box::into_raw(hr_shape_plan) as *mut c_void
}

#[no_mangle]
pub unsafe extern "C" fn _hb_harfrust_shape_plan_destroy_rs(data: *mut c_void) {
    let data = data as *mut harfrust::ShapePlan;
    let _hr_shape_plan = Box::from_raw(data);
}

#[no_mangle]
pub unsafe extern "C" fn _hb_harfrust_shape_rs(
    font_data: *const c_void,
    face_data: *const c_void,
    shape_plan: *const c_void,
    hr_buffer_box: *const c_void,
    font: *mut hb_font_t,
    buffer: *mut hb_buffer_t,
    pre_context: *const u8,
    pre_context_length: u32,
    post_context: *const u8,
    post_context_length: u32,
    features: *const hb_feature_t,
    num_features: u32,
) -> hb_bool_t {
    let font_data = font_data as *const HBHarfRustFontData;
    let face_data = face_data as *const HBHarfRustFaceData;

    let font_ref = &(*face_data).font_ref;

    let hr_buffer_box = hr_buffer_box as *mut harfrust::UnicodeBuffer;
    let mut hr_buffer_box = Box::from_raw(hr_buffer_box);
    let mut hr_buffer = *hr_buffer_box;

    // Set buffer properties
    let cluster_level = hb_buffer_get_cluster_level(buffer);
    let cluster_level = match cluster_level {
        hb_buffer_cluster_level_t_HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES => {
            harfrust::BufferClusterLevel::MonotoneGraphemes
        }
        hb_buffer_cluster_level_t_HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS => {
            harfrust::BufferClusterLevel::MonotoneCharacters
        }
        hb_buffer_cluster_level_t_HB_BUFFER_CLUSTER_LEVEL_CHARACTERS => {
            harfrust::BufferClusterLevel::Characters
        }
        hb_buffer_cluster_level_t_HB_BUFFER_CLUSTER_LEVEL_GRAPHEMES => {
            harfrust::BufferClusterLevel::Graphemes
        }
        _ => harfrust::BufferClusterLevel::default(),
    };
    hr_buffer.set_cluster_level(cluster_level);
    let flags = hb_buffer_get_flags(buffer);
    hr_buffer.set_flags(harfrust::BufferFlags::from_bits_truncate(flags));
    let not_found_variation_selector_glyph =
        hb_buffer_get_not_found_variation_selector_glyph(buffer);
    if not_found_variation_selector_glyph != u32::MAX {
        hr_buffer.set_not_found_variation_selector_glyph(not_found_variation_selector_glyph);
    }

    // Segment properties:
    let script = hb_buffer_get_script(buffer);
    let language = hb_buffer_get_language(buffer);
    let direction = hb_buffer_get_direction(buffer);
    // Convert to HarfRust types
    let script = harfrust::Script::from_iso15924_tag(Tag::from_u32(script))
        .unwrap_or(harfrust::script::UNKNOWN);
    let language = hb_language_to_hr_language(language);
    let direction = match direction {
        hb_direction_t_HB_DIRECTION_LTR => harfrust::Direction::LeftToRight,
        hb_direction_t_HB_DIRECTION_RTL => harfrust::Direction::RightToLeft,
        hb_direction_t_HB_DIRECTION_TTB => harfrust::Direction::TopToBottom,
        hb_direction_t_HB_DIRECTION_BTT => harfrust::Direction::BottomToTop,
        _ => harfrust::Direction::Invalid,
    };
    // Set properties on the buffer
    hr_buffer.set_script(script);
    if let Some(lang) = language {
        hr_buffer.set_language(lang);
    }
    hr_buffer.set_direction(direction);

    // Populate buffer
    let count = hb_buffer_get_length(buffer);
    let infos = hb_buffer_get_glyph_infos(buffer, null_mut());

    hr_buffer.reserve(count as usize);

    for i in 0..count {
        let info = &*infos.add(i as usize);
        let unicode = info.codepoint;
        let cluster = info.cluster;
        hr_buffer.add(char::from_u32_unchecked(unicode), cluster);
    }

    let pre_context = std::slice::from_raw_parts(pre_context, pre_context_length as usize);
    hr_buffer.set_pre_context(str::from_utf8(pre_context).unwrap());
    let post_context = std::slice::from_raw_parts(post_context, post_context_length as usize);
    hr_buffer.set_post_context(str::from_utf8(post_context).unwrap());

    let ptem = hb_font_get_ptem(font);
    let ptem = if ptem > 0.0 { Some(ptem) } else { None };

    let shaper = if ptem.is_some() {
        (*face_data)
            .shaper_data
            .shaper(font_ref)
            .instance(Some(&(*font_data).shaper_instance))
            .point_size(ptem)
            .build()
    } else {
        (*font_data).shaper.clone()
    };

    let features = if features.is_null() {
        Vec::new()
    } else {
        let features = std::slice::from_raw_parts(features, num_features as usize);
        features
            .iter()
            .map(|f| {
                let tag = f.tag;
                let value = f.value;
                let start = f.start;
                let end = f.end;
                harfrust::Feature {
                    tag: Tag::from_u32(tag),
                    value,
                    start,
                    end,
                }
            })
            .collect::<Vec<_>>()
    };

    let glyphs = if shape_plan.is_null() {
        shaper.shape(hr_buffer, &features)
    } else {
        let shape_plan = shape_plan as *const harfrust::ShapePlan;
        shaper.shape_with_plan(shape_plan.as_ref().unwrap(), hr_buffer, &features)
    };

    let count = glyphs.len();
    hb_buffer_set_length(buffer, 0u32);
    hb_buffer_set_content_type(
        buffer,
        hb_buffer_content_type_t_HB_BUFFER_CONTENT_TYPE_GLYPHS,
    );
    hb_buffer_set_length(buffer, count as u32);
    let mut count_out: u32 = 0;
    let infos = hb_buffer_get_glyph_infos(buffer, &mut count_out);
    let positions = hb_buffer_get_glyph_positions(buffer, null_mut());
    if count != count_out as usize {
        return false as hb_bool_t;
    }

    let mut x_scale: i32 = 0;
    let mut y_scale: i32 = 0;
    hb_font_get_scale(font, &mut x_scale, &mut y_scale);
    let upem = shaper.units_per_em();
    let upem = if upem > 0 { upem } else { 1000 };
    let x_mult = if x_scale < 0 {
        -((-x_scale as i64) << 16)
    } else {
        (x_scale as i64) << 16
    } / upem as i64;
    let y_mult = if y_scale < 0 {
        -((-y_scale as i64) << 16)
    } else {
        (y_scale as i64) << 16
    } / upem as i64;

    let em_mult =
        |v: i32, mult: i64| -> hb_position_t { ((v as i64 * mult + 32768) >> 16) as hb_position_t };

    for (i, (hr_info, hr_pos)) in glyphs
        .glyph_infos()
        .iter()
        .zip(glyphs.glyph_positions())
        .enumerate()
    {
        let info = &mut *infos.add(i);
        let pos = &mut *positions.add(i);
        info.codepoint = hr_info.glyph_id;
        info.cluster = hr_info.cluster;
        info.mask = 0;
        if hr_info.unsafe_to_break() {
            info.mask |= hb_glyph_flags_t_HB_GLYPH_FLAG_UNSAFE_TO_BREAK;
        }
        if hr_info.unsafe_to_concat() {
            info.mask |= hb_glyph_flags_t_HB_GLYPH_FLAG_UNSAFE_TO_CONCAT;
        }
        if hr_info.safe_to_insert_tatweel() {
            info.mask |= hb_glyph_flags_t_HB_GLYPH_FLAG_SAFE_TO_INSERT_TATWEEL;
        }
        pos.x_advance = em_mult(hr_pos.x_advance, x_mult);
        pos.y_advance = em_mult(hr_pos.y_advance, y_mult);
        pos.x_offset = em_mult(hr_pos.x_offset, x_mult);
        pos.y_offset = em_mult(hr_pos.y_offset, y_mult);
    }

    let hr_buffer = glyphs.clear();
    *hr_buffer_box = hr_buffer; // Move the buffer back into the box
    let _ = Box::into_raw(hr_buffer_box); // Prevent double free

    true as hb_bool_t
}
