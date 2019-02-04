DEF_CF( cfunc_draw,         "draw prog\n" )
DEF_CF( cfunc_play,         "play n\n" )
DEF_CF( cfunc_stop,         "stop n\n" )
DEF_CF( cfunc_set_volume,   "set-volume n b\n" )
DEF_CF( cfunc_show,         "show wid\n" )
DEF_CF( cfunc_hide,         "hide wid\n" )
DEF_CF( cfunc_visibleQ,     "visible? wid\n" )
DEF_CF( cfunc_move,         "move wid pos /center\n" )
DEF_CF( cfunc_resize,       "resize wid a\n" )
DEF_CF( cfunc_text_size,    "text-size f text\n" )
DEF_CF( uc_handle_events,   "handle-events wid /wait\n" )
DEF_CF( uc_clear_color,     "clear-color color\n" )
DEF_CF( uc_display_swap,    "display-swap\n" )
DEF_CF( uc_display_area,    "display-area\n" )
DEF_CF( uc_display_snap,    "display-snapshot\n" )
DEF_CF( uc_display_cursor,  "display-cursor\n" )
DEF_CF( uc_key_repeat,      "key-repeat\n" )
DEF_CF( cfunc_key_code,     "key-code\n" )
DEF_CF( cfunc_load_png,     "load-png f\n" )
DEF_CF( cfunc_save_png,     "save-png f rast\n" )
DEF_CF( cfunc_buffer_audio, "buffer-audio a\n" )
DEF_CF( cfunc_display,      "display size /fullscreen /title text\n" )
DEF_CF( cfunc_to_degrees,   "to-degrees n\n" )
DEF_CF( cfunc_to_radians,   "to-radians n\n" )
DEF_CF( cfunc_limit,        "limit n min max\n" )
DEF_CF( cfunc_look_at,      "look-at a b\n" )
DEF_CF( cfunc_turntable,    "turntable c a\n" )
DEF_CF( cfunc_lerp,         "lerp a b f\n" )
DEF_CF( cfunc_curve_at,     "curve-at a b\n" )
DEF_CF( cfunc_animate,      "animate a time\n" )
DEF_CF( cfunc_blit,         "blit a b pos\n" )
DEF_CF( cfunc_move_glyphs,  "move-glyphs f pos\n" )
DEF_CF( cfunc_point_in,     "point-in a pnt\n" )
DEF_CF( cfunc_pick_point,   "pick-point a c pnt pos\n" )
DEF_CF( cfunc_change_vbo,   "change-vbo a b n\n" )
DEF_CF( cfunc_make_sdf,     "make-sdf rast raster! m int! b double!\n" )
DEF_CF( uc_gl_extensions,   "gl-extensions\n" )
DEF_CF( uc_gl_version,      "gl-version\n" )
DEF_CF( uc_gl_max_textures, "gl-max-textures\n" )
DEF_CF( cfunc_shadowmap,    "shadowmap size\n" )
DEF_CF( cfunc_distance,     "distance a b\n" )
DEF_CF( cfunc_dot,          "dot a b\n" )
DEF_CF( cfunc_cross,        "cross a b\n" )
DEF_CF( cfunc_normalize,    "normalize vec\n" )
DEF_CF( cfunc_project_point,"project-point pnt a b\n" )
DEF_CF( cfunc_set_matrix,   "set-matrix m q\n" )
DEF_CF( cfunc_mul_matrix,   "mul-matrix m b\n" )

#ifdef TIMER_GROUP
    DEF_CF( uc_timer_group,    "timer-group\n" )
    DEF_CF( uc_add_timer,      "add-timer\n" )
    DEF_CF( uc_check_timers,   "check-timers\n" )
#endif
/*
DEF_CF( cfunc_particle_sim,"particle-sim vec prog n" )
DEF_CF( uc_perlin,         "perlin" )
*/
