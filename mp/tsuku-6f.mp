%
% Unicode page 6F (Kanji) for Tsukurimashou
% Copyright (C) 2011, 2012  Matthew Skala
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
input bottomrad.mp;
input buildkanji.mp;
input gradefour.mp;
input gradeone.mp;
input gradetwo.mp;
input leftrad.mp;
input toprad.mp;

do_late_includes;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grfour.go_fish =
  push_pbox_toexpand("kanji.grfour.go_fish");
  build_kanji.lr(270,-10)
    (kanji.leftrad.water)
    (kanji.grtwo.fish);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grsix.tide =
  push_pbox_toexpand("kanji.grsix.tide");
  build_kanji.lr(300,30)
    (kanji.leftrad.water)
    (kanji.grtwo.morning);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% gyo/asa "go fish"
begintsuglyph("uni6F01",1);
  kanji.grfour.go_fish;
  tsu_render;
endtsuglyph;

% kan "Chinese"
begintsuglyph("uni6F22",34);
  build_kanji.level(build_kanji.lr(360,10)
    (kanji.leftrad.water)
    (build_kanji.tb(630,-10)
       (kanji.toprad.grass)
       (build_kanji.box((210,800),(790,570));
        tsu_xform(identity yscaled 0.65 shifted (-10,0))
          (kanji.grfour.husband);
        replace_strokep(-1)((500,800)--oldp);
        replace_strokeq(-1)((1.6,1.6)--oldq);
        set_boserif(-1,0,whatever))));
  tsu_render;
endtsuglyph;

% chou/shio "tide" (or "opportunity")
begintsuglyph("uni6F6E",110);
  kanji.grsix.tide;
  tsu_render;
endtsuglyph;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

endfont;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
