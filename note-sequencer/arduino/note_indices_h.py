print(
    """
#pragma once
"""
)

note_names = [
    "C ",
    "C#",
    "D ",
    "D#",
    "E ",
    "F ",
    "F#",
    "G ",
    "G#",
    "A ",
    "A#",
    "B ",
]

num_octaves = 10

i = 0

for octave in range(0, num_octaves):
    for note_name in note_names:
        note_to_display = "{note_name}{octave}".format(
            note_name=note_name, octave=octave
        )
        note_for_constant_name = note_to_display.replace("#", "_SHARP_").replace(
            " ", "_"
        )
        print(
            "#define NOTE_{note_for_constant_name} {i}".format(
                note_for_constant_name=note_for_constant_name, i=i
            )
        )
        i += 1
