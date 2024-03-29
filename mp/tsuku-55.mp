%
% Unicode page 55 (Kanji) for Tsukurimashou
% Copyright (C) 2011, 2012, 2013, 2014  Matthew Skala
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
input bkencl.mp;
input buildkanji.mp;
input gradeeight.mp;
input gradefour.mp;
input gradenine.mp;
input gradeone.mp;
input gradethree.mp;
input gradetwo.mp;
input leftrad.mp;
input radical.mp;
input rare.mp;
input toprad.mp;

do_late_includes;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grthree.question =
  push_pbox_toexpand("kanji.grthree.question");
  build_kanji.gate_enclose(
    build_kanji.sscale(scaled 0.8)(kanji.grone.mouth));
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grfour.chant =
  push_pbox_toexpand("kanji.grfour.chant");
  build_kanji.lr(320,30)
    (kanji.leftrad.mouth)
    (kanji.grnine.prosperity);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grfive.business =
  push_pbox_toexpand("kanji.grfive.business");
  build_kanji.tb(740,100)
    (kanji.toprad.alt_sparkle_three)
    (build_kanji.tb(580,110)
       (kanji.radical.conservative_hat)
       (kanji.greight.spine));
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% shou/tona "chant"
begintsuglyph("uni5531",49);
  kanji.grfour.chant;
  tsu_render;
endtsuglyph;

% shou/akina
begintsuglyph("uni5546",70);
  kanji.grthree.commerce;
  tsu_render;
endtsuglyph;

% mon/to "question"
begintsuglyph("uni554F",79);
  kanji.grthree.question;
  tsu_render;
endtsuglyph;

% ki/yoroko "pleasure"
begintsuglyph("uni559C",156);
  kanji.grfour.pleasure;
  tsu_render;
endtsuglyph;

% kyou/taka "boast"
begintsuglyph("uni55AC",172);
  kanji.grnine.boast;
  tsu_render;
endtsuglyph;

% ei/itona "business"
begintsuglyph("uni55B6",182);
  kanji.grfive.business;
  tsu_render;
endtsuglyph;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

endfont;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
