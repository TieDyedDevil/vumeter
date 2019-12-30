# vumeter

VU meter for PulseAudio.

A VU meter's reading corresponds approximately to the perceptual response of
the human ear. This implementation differs from an analog VU meter in that
it has a wider effective dynamic range and a full-scale value corresponding
to digital 0 dB. The meter's ballistic property matches an analog VU meter
in that it takes the needle approximately 300 milliseconds to go from
no-signal to 0 dB upon application of a full-scale signal.

It is important that you understand that this meter is not suited to setting
levels for a digital recording; use a peak program meter. This meter is best
suited for evaluating perceptual program levels during playback.

This code descends from original work by Martin Cameron.
