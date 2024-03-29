%
% Unicode page 56 (Kanji) for Tsukurimashou
% Copyright (C) 2011, 2012, 2013  Matthew Skala
%
% This program is free software: you can redistribute it and/or modify
% it under the terms of the GNU General Public License as published by
% the Free Software Foundation, version 3.
%
% As a special exception, if you create a document which uses this font, and
% embed this font or unaltered portions of this font into the document, this
% font does not by itself cause the resulting document to be covered by the
% GNU General Public License. This exception does not however invalidate any
% other reasons why the document might be covered by the GNU General Public
% License. If you modify this font, you may extend this exception to your
% version of the font, but you are not obligated to do so. If you do not
% wish to do so, delete this exception statement from your version.
%
% This program is distributed in the hope that it will be useful,
% but WITHOUT ANY WARRANTY; without even the implied warranty of
% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
% GNU General Public License for more details.
%
% You should have received a copy of the GNU General Public License
% along with this program.  If not, see <http://www.gnu.org/licenses/>.
%
% Matthew Skala
% http://ansuz.sooke.bc.ca/
% mskala@ansuz.sooke.bc.ca
%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

beginfont

% AUTODEPS
input buildkanji.mp;
input gradeeight.mp;
input gradefive.mp;
input gradefour.mp;
input gradeone.mp;
input gradesix.mp;
input gradetwo.mp;
input radical.mp;

do_late_includes;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grtwo.country =
  push_pbox_toexpand("kanji.grtwo.country");
  kanji.radical.box;
  build_kanji.sscale(scaled 0.75)(kanji.grone.ball);
  expand_pbox;
enddef;

vardef kanji.grtwo.map =
  push_pbox_toexpand("kanji.grtwo.map");
  kanji.radical.box;
  push_stroke((270,650)..tension 1.2..(340,560)..(360,500),
    (1,1)--(1.2,1.2)--(1.3,1.3));
  push_stroke(get_strokep(0) shifted ((430,690)-point 0 of get_strokep(0)),
    get_strokeq(0));
  push_stroke((760,610)..tension 1.2..(570,290)..(210,100),
    (1.6,1.6)--(1.4,1.4)--(1.1,1.1));
  push_stroke((240,430)..tension 1.2..(460,310)..(710,80),
    (1.2,1.2)--(1.4,1.4)--(1.5,1.5));
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grfour.enclosure =
  push_pbox_toexpand("kanji.grfour.enclosure");
  kanji.radical.box;
  build_kanji.sscale(scaled 0.77 shifted (0,50))(kanji.greight.well);
  expand_pbox;
enddef;

vardef kanji.grfour.utensil =
  push_pbox_toexpand("kanji.grfour.utensil");
  build_kanji.sscale(yscaled 0.44)
    (build_kanji.spread_legs(140)(kanji.grone.big));
  build_kanji.box((160,790),(430,590));
  build_kanji.box((570,790),(840,590));
  build_kanji.box((150,200),(430,-20));
  build_kanji.box((570,200),(850,-20));
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grfive.brigade =
  push_pbox_toexpand("kanji.grfive.brigade");
  kanji.radical.box;
  build_kanji.sscale(scaled 0.75)(kanji.grsix.inch);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% ki/utsuwa "utensil"
begintsuglyph("uni5668",104);
  kanji.grfour.utensil;
  tsu_render;
endtsuglyph;

% box radical
begintsuglyph("uni56D7",215);
  kanji.radical.box;
  tsu_render;
endtsuglyph;

% shi/yon "four"
begintsuglyph("uni56DB",219);
  kanji.grone.four;
  tsu_render;
endtsuglyph;

% kai "counter for occurrences"
begintsuglyph("uni56DE",222);
  kanji.grtwo.occurrences;
  tsu_render;
endtsuglyph;

% in/yo "cause"
begintsuglyph("uni56E0",224);
  kanji.grfive.cause;
  tsu_render;
endtsuglyph;

% dan/katamari "group or association" (as in SOS Brigade)
begintsuglyph("uni56E3",227);
  kanji.grfive.brigade;
  tsu_render;
endtsuglyph;

% extra
begintsuglyph("uni56ED",237);
  kanji.radical.box;
  build_kanji.sscale(scaled 0.75)(kanji.grtwo.origin);
  tsu_render;
endtsuglyph;

% koma "bothered"
begintsuglyph("uni56F0",240);
  kanji.grsix.bothered;
  tsu_render;
endtsuglyph;

% i/kako "enclosure"
begintsuglyph("uni56F2",242);
  kanji.grfour.enclosure;
  tsu_render;
endtsuglyph;

% zu/haka "map"
begintsuglyph("uni56F3",243);
  kanji.grtwo.map;
  tsu_render;
endtsuglyph;

% ko/kata "hard"
begintsuglyph("uni56FA",250);
  kanji.grfour.hard;
  tsu_render;
endtsuglyph;

% koku/kuni "country"
begintsuglyph("uni56FD",253);
  kanji.grtwo.country;
  tsu_render;
endtsuglyph;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

endfont;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
