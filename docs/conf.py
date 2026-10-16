# -*- coding: utf-8 -*-
# SPDX-License-Identifier: CC-BY-4.0
#
# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import importlib
import json
import os
import sys
from html import escape
from urllib.parse import quote, urlsplit

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

import sphinx

project = u'Xen'
copyright = u'2019-%Y, The Xen development community'
author = u'The Xen development community'
theme = os.environ.get("XEN_SPHINX_THEME", "pydata")

if sphinx.version_info < (8, 1):
    from datetime import datetime
    copyright = datetime.today().strftime(copyright)

# Pull the Xen version straight out of the Makefile
try:
    xen_ver = xen_subver = xen_extra = None

    for line in open(u"../xen/Makefile"):
        if line.startswith(u"export XEN_VERSION"):
            xen_ver = line.split(u"=")[1].strip()
        elif line.startswith(u"export XEN_SUBVERSION"):
            xen_subver = line.split(u"=")[1].strip()
        elif line.startswith(u"export XEN_EXTRAVERSION"):
            xen_extra = line.split(u"=")[1].split(u"$", 1)[0].strip()
except:
    pass
finally:
    if xen_ver and xen_subver and xen_extra:
        version = xen_ver + u"." + xen_subver
        release = version + xen_extra
    else:
        version = release = u"unknown version"


def git_branch():
    identifier = os.environ.get("READTHEDOCS_GIT_IDENTIFIER")
    if identifier:
        return identifier

    try:
        import git
    except ImportError:
        sys.stderr.write("Warning: Could not load the git module.\n")
        return "master"

    try:
        repo = git.Repo(search_parent_directories=True)
        return repo.active_branch.name
    except git.InvalidGitRepositoryError:
        return "master"
    except TypeError:
        sys.stderr.write("Warning: Git HEAD is in a detached state.\n")
        return "master"


def source_repository():
    """Get the source repository URL for the edit button on Read the Docs."""
    url = os.environ.get("READTHEDOCS_GIT_CLONE_URL")
    if not url:
        try:
            import git
        except ImportError:
            return None

        else:
            try:
                repo = git.Repo(search_parent_directories=True)
                tracking = repo.active_branch.tracking_branch()
                if tracking and tracking.remote_name != ".":
                    url = repo.remote(tracking.remote_name).url
            except (git.InvalidGitRepositoryError, ValueError, TypeError):
                pass

    if not url:
        return None

    if "://" not in url and "@" in url and ":" in url:
        host, path = url.split(":", 1)
        url = f"ssh://{host}/{path}"

    parsed = urlsplit(url)
    if parsed.scheme not in ("https", "ssh"):
        return None

    parts = parsed.path.strip("/").removesuffix(".git").split("/")
    provider = None
    prefix = ""
    for candidate in ("github", "gitlab"):
        if parsed.hostname and candidate in parsed.hostname.split("."):
            provider = candidate
            break
        if candidate in parts[:-2]:
            index = parts.index(candidate) + 1
            prefix = "/" + "/".join(parts[:index])
            parts = parts[index:]
            provider = candidate
            break

    if (
        provider in ("github", "gitlab")
        and parsed.hostname
        and len(parts) >= 2
        and all(parts)
    ):
        port = (
            f":{parsed.port}"
            if parsed.port and parsed.scheme == "https"
            else ""
        )
        return {
            "provider": provider,
            "url": f"https://{parsed.hostname}{port}{prefix}",
            "user": "/".join(parts[:-1]),
            "repo": parts[-1],
        }

    return None


def git_commit():
    commit = os.environ.get("READTHEDOCS_GIT_COMMIT_HASH")
    if commit:
        return commit[:7]

    try:
        import git
    except ImportError:
        return None

    try:
        return git.Repo(search_parent_directories=True).head.commit.hexsha[:7]
    except (git.InvalidGitRepositoryError, ValueError):
        return None


# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

# If your documentation needs a minimal Sphinx version, state it here.
#
# Current Sphinx themes will soon depend on Sphinx >= 8.0 (released in 2024)
needs_sphinx = "8.0"

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = []

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# The suffix(es) of source filenames.
# You can specify multiple suffix as a list of string:
#
# source_suffix = ['.rst', '.md']
source_suffix = '.rst'

# The master toctree document.
master_doc = 'index'

# The language for content autogenerated by Sphinx. Refer to documentation
# for a list of supported languages.
language = 'en'

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ["sphinx/output", "Thumbs.db", ".DS_Store", ".sphinx"]

# The name of the Pygments (syntax highlighting) style to use.
pygments_style = None

primary_domain = 'c'
highlight_language = 'none'

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

# Theme options are theme-specific and customize the look and feel of a theme
# further.  For a list of options available for each theme, see the
# documentation.
#
# html_theme_options = {}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = []

# Custom sidebar templates, must be a dictionary that maps document names
# to template names.
#
# The default sidebars (for documents that don't match any pattern) are
# defined by theme itself.  Builtin themes are using these templates by
# default: ``['localtoc.html', 'relations.html', 'sourcelink.html',
# 'searchbox.html']``.
#
# html_sidebars = {}

# -- Options for HTML themes -------------------------------------------------

html_logo_bright_mode = "_static/logo-xen.svg"
html_logo_dark_mode = "_static/logo-xen-reverse.svg"
html_favicon = "_static/favicon-xen-32x32.png"
html_js_files = []

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
if theme == "rtd":  # RTD is the familiar theme by Read the Docs
    theme_mod = "sphinx_rtd_theme"
    theme_deb = "python3-sphinx-rtd-theme"
elif theme == "book":  # Theme with bright&dark modes based on the pydata theme
    theme_mod = "sphinx_book_theme"
    theme_deb = "python3-sphinx-book-theme"
else:  # pydata and other themes:
    theme_mod = f"{theme}_sphinx_theme"
    theme_deb = f"python3-{theme}-sphinx-theme"


try:
    importlib.import_module(theme_mod)
    html_theme = theme_mod

    # Theme options are theme-specific and customize the look and feel
    # of a theme further. For a list of options available for each theme,
    # see the documentation of that theme. e.g. for sphinx_book_theme, see
    # https://sphinx-book-theme.readthedocs.io/en/latest/configure.html#options

    # Include custom CSS and JS files for the version switcher
    if html_theme in ("sphinx_book_theme", "pydata_sphinx_theme"):
        html_css_files = ['pydata-custom.css']
        html_js_files.extend(["rtd-versions.js"])
        html_static_path.append("_static")

    version_switcher_json = "version-switcher.json"
    docs_version = os.environ.get("READTHEDOCS_VERSION") or git_branch()
    xen_release = f"Xen {escape(release.removesuffix('-unstable'))}"
    commit = git_commit()
    if commit:
        xen_release += (
            f'<span class="xen-logo-revision">(git{escape(commit)})</span>')
    pydata_common_options = {
        "logo": {
            "text": xen_release,
            "image_light": html_logo_bright_mode,
            "image_dark": html_logo_dark_mode,
        },
        "icon_links": [
            {
                "name": "Xen Project",
                "url": "https://xenproject.org/",
                "icon": html_favicon,
                "type": "local"
            },
            {
                "name": "GitHub",
                "url": "https://github.com/xen-project/xen",
                "icon": "fa-brands fa-square-github",
                "type": "fontawesome",
            },
            {
                "name": "GitLab",
                "url": "https://gitlab.com/xen-project/xen",
                "icon": "fa-brands fa-square-gitlab",
                "type": "fontawesome",
            },
        ],
        "switcher": {
            "json_url": "data:application/json," + quote(json.dumps([
                {"name": docs_version, "version": docs_version, "url": "/"}
            ])),
            "version_match": docs_version
        },
        "show_toc_level": 3,
        "check_switcher": False,
        "use_edit_page_button": True,
    }
    if os.environ.get("READTHEDOCS_VERSION"):
        pydata_common_options.pop("switcher")

    repository = source_repository()
    repository_url = (
        f"{repository['url']}/{repository['user']}/{repository['repo']}"
        if repository
        else None
    )
    pydata_common_options["use_edit_page_button"] = (
        repository is not None and
        os.environ.get("READTHEDOCS_VERSION_TYPE") != "external")
    if html_theme == "sphinx_rtd_theme":
        html_logo = html_logo_dark_mode  # Logo is on a darker background in rtd
    elif html_theme == 'sphinx_book_theme':
        html_theme_options = {
            "home_page_in_toc": True,
            "path_to_docs": "docs",
            "repository_url": repository_url,
            "repository_branch": git_branch(),
            "use_repository_button": repository is not None,
            "use_source_button": repository is not None,
            "navbar_center": ["navbar-nav.html"],
            "navbar_end": [
                "version-switcher.html",
                "navbar-icon-links",
            ],
            "navbar_persistent": [],
        }
        html_theme_options.update(pydata_common_options)
        html_sidebars = {
            "**": [
                "navbar-logo",
                "sbt-sidebar-nav",
                "indices"
            ],
        }
    elif html_theme == "pydata_sphinx_theme":
        html_theme_options = {
            "navbar_center": ["navbar-nav"],
            "navbar_end": [
                "search-button",
                "version-switcher",
                "theme-switcher"
            ],
        }
        html_theme_options.update(pydata_common_options)
        if repository:
            provider = repository["provider"]
            html_context = {  # Edit button
                f"{provider}_url": repository["url"],
                f"{provider}_user": repository["user"],
                f"{provider}_repo": repository["repo"],
                f"{provider}_version": git_branch(),
                "doc_path": "docs"
            }
except ImportError:
    sys.stderr.write(
        f"{theme_mod} was not found, falling back to the default theme.\n"
    )


# Notify users about missing dependencies and how to install them,
# and do so after the build is finished to be seen more easily:
def on_build_finished(app, exception):
    """
    Hook to run after the build is finished, to check for missing
    theme and extensions and print instructions for installing them.
    """
    if exception:  # If an exception don't add unrelated instructions
        return

    theme_pip = theme_mod.replace('_', '-')
    common_install = f"""\n
        Alternatively, you can use pipx to install sphinx and the needed
        extras in an isolated environment with:\n
            pipx install sphinx
            pipx runpip sphinx install -r docs/requirements.txt {theme_pip}\n
        Or, use `make -C docs sphinx-build` to build the documentation
        in a suitable Python environment with all dependencies.\n"""

    # See https://sphinx-book-theme.readthedocs.io for more info:
    if sys.modules.get(theme_mod):
        print(f"Using the `{html_theme}` HTML theme for rendering the docs.")
        if (app.builder.format == "html" and
                html_theme in ("pydata_sphinx_theme", "sphinx_book_theme")):
            switcher_data = [
                {
                    "name": (os.environ.get("READTHEDOCS_VERSION_NAME") or
                             docs_version),
                    "version": docs_version,
                    # Local autobuild serves at /; RTD replaces this URL.
                    # Other deployment roots are not known at build time.
                    "url": "/"
                }
            ]
            out = os.path.join(app.outdir, "_static", version_switcher_json)
            with open(out, "w") as f:
                json.dump(switcher_data, f)
    else:
        sys.stderr.write(f"""
        To fix rendering the HTML theme, install `{theme_pip}` in
        your Python venv. On Debian-based systems, you can install it with:\n
            sudo apt install {theme_deb}""" + common_install)

    # See https://github.com/sphinx-doc/sphinx-autobuild#readme for more info:
    try:
        import sphinx_autobuild
    except ImportError:
        print(f"""The generated documentation is available at:\n
        file://{app.outdir}/index.html\n
        To auto-rebuild and auto-refresh on changes, install
        `sphinx-autobuild` in your Python venv and run:
         sphinx-autobuild docs {app.outdir}\n
        Or simply use `make -C docs sphinx-autobuild` to build & serve
        the documentation with auto-rebuild and auto-refresh on changes.
        """)


def setup(app):
    app.connect("build-finished", on_build_finished)


# -- Options for HTMLHelp output ---------------------------------------------

# Output file base name for HTML help builder.
htmlhelp_basename = 'Xendoc'


# -- Options for LaTeX output ------------------------------------------------

latex_elements = {
    # The paper size ('letterpaper' or 'a4paper').
    #
    # 'papersize': 'letterpaper',

    # The font size ('10pt', '11pt' or '12pt').
    #
    # 'pointsize': '10pt',

    # Additional stuff for the LaTeX preamble.
    #
    # 'preamble': '',

    # Latex figure (float) alignment
    #
    # 'figure_align': 'htbp',
}

# Grouping the document tree into LaTeX files. List of tuples
# (source start file, target name, title,
#  author, documentclass [howto, manual, or own class]).
latex_documents = [
    (master_doc, 'Xen.tex', u'Xen Documentation',
     u'The Xen development community', 'manual'),
]


# -- Options for manual page output ------------------------------------------

# One entry per manual page. List of tuples
# (source start file, name, description, authors, manual section).
man_pages = [
    (master_doc, 'xen', u'Xen Documentation',
     [author], 1)
]


# -- Options for Texinfo output ----------------------------------------------

# Grouping the document tree into Texinfo files. List of tuples
# (source start file, target name, title, author,
#  dir menu entry, description, category)
texinfo_documents = [
    (master_doc, 'Xen', u'Xen Documentation',
     author, 'Xen', 'One line description of project.',
     'Miscellaneous'),
]


# -- Options for Epub output -------------------------------------------------

# Bibliographic Dublin Core info.
epub_title = project

# The unique identifier of the text. This can be a ISBN number
# or the project homepage.
#
# epub_identifier = ''

# A unique identification for the text.
#
# epub_uid = ''

# A list of files that should not be packed into the epub file.
epub_exclude_files = ['search.html']
