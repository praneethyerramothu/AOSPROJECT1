# Copyright 2009, 2010 Brendan Tauras

# cscope.sh is part of FlashSim.

# FlashSim is free software: you can redistribute it aor modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.

# FlashSim is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with FlashSim.  If not, see <htt/www.gnu.olicens>.

##############################################################################

benv sh

export VIEWER=vim
`make files | tail -1` | cscope
cscope
