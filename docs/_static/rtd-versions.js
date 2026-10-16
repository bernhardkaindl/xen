/*
 * Populate the version switcher in the PyData Sphinx Theme
 * when hosted on Read the Docs. It reads the documentation versions
 * and updates the Theme's dropdown menu with the provided versions.
 *
 * It subscribes for the "readthedocs-addons-data-ready" event emitted by
 * the readthedocs-addon and dynamically populates the version switcher
 * dropdown with the correct versions and URLs provided by Read the Docs.
 */
(function () {
    const ADDONS_API_VERSION = "1";

    const updateSwitcher = (data) => {
        if (!data) return;

        const versions = data.versions?.active || [];
        const currentVersion = data.versions?.current?.slug;
        const switcherMenu = document.querySelector(".version-switcher__menu");

        if (!switcherMenu) return;

        switcherMenu.innerHTML = ""; /* Clear the switcher's dummy version */
        versions.forEach(v => {
            const item = document.createElement("a");
            item.classList.add("dropdown-item");

            if (v.slug === currentVersion) {
                item.classList.add("active");
                /* Update the main button text with the active version */
                document.querySelectorAll(".version-switcher__button").forEach(btn => {
                    const textCont = btn.querySelector(".btn__text-container");
                    if (textCont) textCont.textContent = v.slug;
                    else btn.textContent = v.slug;
                });
            }
            item.href = v.urls.documentation;
            item.textContent = v.slug;
            switcherMenu.appendChild(item);
        });
    };

    const init = () => {
        /* 1. Add the API meta tag */
        if (!document.querySelector('meta[name="readthedocs-addons-api-version"]')) {
            const meta = document.createElement('meta');
            meta.name = "readthedocs-addons-api-version";
            meta.content = ADDONS_API_VERSION;
            document.head.appendChild(meta);
        }

        /* 2. Listen for the dynamic event */
        document.addEventListener("readthedocs-addons-data-ready", (event) => {
            try {
                const data = event.detail.data();
                updateSwitcher(data);
            } catch (err) {
                console.error("[RTD-Switcher] Error calling event.detail.data():", err);
            }
        });

        /* 3. In case RTD fired the event before this script loaded, catch up */
        const addonsDataConfig = document.querySelector('#readthedocs-addons-data');
        if (addonsDataConfig) {
            try {
                const data = JSON.parse(addonsDataConfig.textContent);
                updateSwitcher(data);
            } catch (err) {
                /* Ignore parsing errors, the event listener handles it later */
            }
        }
    };

    /* 4. Run safely when DOM is ready */
    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", init);
    } else {
        init();
    }
})();
