#!/usr/bin/octave -qf

data = csvread("pc-depth.csv");

% Produce graph.
%set( 0, 'defaultfigurevisible', 'off' );

y = [];
for x = min(data):max(data)
  y(end+1) = sum(data <= x);
endfor

x = min(data):max(data);

plot(x, y);

print( 'pc-depth.eps', '-S600,400', '-color', '-tight', '-deps' );
