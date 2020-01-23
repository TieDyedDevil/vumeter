# VUmeter 2.0

This is a true VU meter for PulseAudio. (Most so-called VU meters are
in fact peak program meters with a damped decay.)

A VU meter's reading corresponds approximately to the perceptual
response of the human ear. This implementation differs from an analog
VU meter in that it has a wider effective dynamic range and an accurate,
linearized (i.e. a constant 6 dB per major division) scale. The meter's
ballistic property matches an analog VU meter in that it takes the
needle approximately 300 milliseconds to go from no-signal to 0 dB upon
application of a full-scale signal.

It is important that you understand that this meter is not suited to
setting levels for a digital recording; use a peak program meter. This
meter is best suited for evaluating perceptual program levels during
playback.

In addition to the meter, two indicators report signal and peak at levels
greater than -54 dB and -1.5 dB, respectively.

The peak indicator is provided to detect peaks in the program material
which naturally occur as a result of normalizing the audio to a value
approaching 0 dB; when the average volume is below 0 dB, peaks will
still occur in excess of that average value. You should not use this
indicator to set recording levels.

The meter may or may not be affected by volume changes on its monitored
sink. For example, if your sink is called `audio-card`, you'd attach the
meter to `audio-card.monitor`. If the sink has hardware volume control,
as evidenced by the presence of the `HW_VOLUME_CTRL` flag in the listing
obtained from `pactl list sinks`, then the monitor will receive the
same signal level regardless of the sink volume, unless the volume is
zero or muted. If the source does not report the `HW_VOLUME_CTRL` flag,
then the monitor volume will track the sink's volume setting.

In the event that you attach the meter to a virtual sink (created using
a null sink module and one or more loopback modules), be aware that this
connection has a loss of 1 dB (for reasons that are unexplained by the
documentation). You should compensate for this using the meter's makeup
gain option.

You may also use makeup gain to compensate when monitoring a sink
that does not have the `HW_VOLUME_CTRL` and has its volume set to less
than 100%.  Determine the appropriate gain by inspecting the output of
`pactl list sinks`.  Remember that the makeup gain is independent of the
sink volume; if the latter changes, you'll need to create a new meter
with the appropriate makeup gain.

By default, two meters are instantiated. More or fewer meters may be
specified via a command-line option; the logo text of each meter may be
set individually.

This code descends from original work by Martin Cameron.

The program requires the SDL2 and SDL2\_ttf libraries and the Google
Noto fonts.

Build using gcc 9 or clang 9; clang is preferred.
