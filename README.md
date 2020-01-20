# VUmeter 2.0

This is a true VU meter for PulseAudio. (Most so-called VU meters are in
fact peak program meters with a damped decay.)

A VU meter's reading corresponds approximately to the perceptual response of
the human ear. This implementation differs from an analog VU meter in that
it has a wider effective dynamic range and an accurate, linearized scale.
The meter's ballistic property matches an analog VU meter in that it takes
the needle approximately 300 milliseconds to go from no-signal to 0 dB upon
application of a full-scale signal.

It is important that you understand that this meter is not suited to setting
levels for a digital recording; use a peak program meter. This meter is best
suited for evaluating perceptual program levels during playback.

In addition to the meter, two indicators report SIGNAL and PEAK at levels
greater than -56 dB and -1.5 dB, respectively.

The PEAK indicator is provided to detect peaks in the program material which
naturally occur as a result of normalizing the audio to a value approaching
0 dB; when the average volume is below 0 dB, peaks will still occur in excess
of that average value. You should not use this indicator to set recording
levels.

Be aware that metering accuracy depends upon the signal path. In particular,
note that PulseAudio loops are not unity-gain.

By default, two meters are instantiated. More or fewer meters may be specified
via a command-line option; the logo text of each meter may be set individually.

This code descends from original work by Martin Cameron.

The program requires the SDL2 and SDL2\_ttf libraries and the DejaVu fonts.

Build using gcc 9 or clang 9; clang is preferred.
