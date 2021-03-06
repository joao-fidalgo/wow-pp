//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software 
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
// 

#include "pch.h"
#include "page_neighborhood.h"
#include "common/macros.h"

namespace wowpp
{
	namespace paging
	{
		PageNeighborhood::PageNeighborhood(Page &mainPage)
		{
			m_pages.fill(nullptr);
			setPageByRelativePosition(PagePosition(0, 0), &mainPage);
		}

		void PageNeighborhood::setPageByRelativePosition(const PagePosition &position, Page *page)
		{
			std::size_t index = toIndex(position);
			ASSERT(index < m_pages.size() && "Index out of range");

			m_pages[index] = page;
		}

		Page * PageNeighborhood::getPageByRelativePosition(const PagePosition &position) const
		{
			return m_pages[toIndex(position)];
		}

		Page & PageNeighborhood::getMainPage() const
		{
			return *getPageByRelativePosition(PagePosition(0, 0));
		}

		std::size_t PageNeighborhood::toIndex(const PagePosition &position)
		{
			return (position[1] * 2 + position[0]);
		}

	}
}
