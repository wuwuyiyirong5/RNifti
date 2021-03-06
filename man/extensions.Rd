% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/extensions.R
\name{extensions}
\alias{extensions}
\alias{extension}
\alias{extensions<-}
\alias{extension<-}
\title{NIfTI extensions}
\usage{
extensions(image)

extension(image, code, mode = c("raw", "character", "numeric", "double",
  "integer", "logical", "complex"), ..., simplify = TRUE)

extensions(image) <- value

extension(image, code) <- value
}
\arguments{
\item{image}{An image, in any acceptable form (see \code{\link{asNifti}}).}

\item{code}{Integer value, expression or string specifying which extension
code is required.}

\item{mode}{The required mode of the extracted data.}

\item{...}{Additional arguments to \code{\link{readBin}}.}

\item{simplify}{Logical value. If \code{TRUE}, the default, a single
extension will be returned as a vector; otherwise a list is always
returned.}

\item{value}{New value for the extension(s).}
}
\value{
For \code{extensions}, a list of raw vectors containing the bytes
  stored in each available header. For \code{extension}, a list of vector
  of values, converted to the required mode, for the extension code
  specified. If the extension code is not used in the image, the return
  value is \code{NULL}. The replacement forms return the modified image.
}
\description{
The NIfTI-1 and NIfTI-2 formats have a simple extension mechanism that
allows additional metadata to be stored with their headers. The format of
this extension data is unspecified by the NIfTI standard, but extension
codes indicate what type of information is present. These functions provide
access to this extension metadata.
}
\details{
The plural version, \code{extensions}, extracts or replaces all extensions
at once. The retrieval form returns a list of raw vectors, each with the
corresponding code in an attribute, and the replacement form accepts a list
of atomic vectors with code attributes, or \code{NULL}, which removes all
extensions. The singular version, \code{extension}, gets all extensions with
the specified code, or appends an extension with that code. Valid extension
codes are stored in the \code{\link{ExtensionCodes}} vector.

NIfTI extensions are stored as a simple, unstructured byte stream, which is
naturally represented in R as a vector of mode \code{"raw"}. However, these
functions will perform some conversion to and from other atomic types for
convenience. The NIfTI standard makes no guarantees about byte order within
the data stream, but the \code{endian} argument to \code{\link{readBin}} can
be passed through when converting to a non-raw type.
}
\seealso{
\code{\link{ExtensionCodes}} for the valid extension codes.
}
\author{
Jon Clayden <code@clayden.org>
}
