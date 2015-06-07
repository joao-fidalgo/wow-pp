
#pragma once

#include "page.h"
#include "page_loader_listener.h"
#include "page_neighborhood.h"
#include <unordered_map>

namespace wowpp
{
	namespace paging
	{
		class LoadedPageSection : public IPageLoaderListener
		{
		public:

			explicit LoadedPageSection(
				PagePosition center,
				std::size_t range,
				IPageLoaderListener &sectionListener);
			
			void updateCenter(const PagePosition &center);

		private:

			typedef std::unordered_map<Page *, PageNeighborhood> PageMap;

			virtual void onPageAvailabilityChanged(const PageNeighborhood &pages, bool isAvailable) override;
			static void setVisibility(PageMap &map, const PageNeighborhood &pages, bool isVisible);

		private:

			PagePosition m_center;
			const std::size_t m_range;
			IPageLoaderListener &m_sectionListener;
			PageMap m_insideOfSection;
			PageMap m_outOfSection;
		};
	}
}
